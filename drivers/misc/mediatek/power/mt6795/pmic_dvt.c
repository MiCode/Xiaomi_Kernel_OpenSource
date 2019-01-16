#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/earlysuspend.h>
#include <linux/seq_file.h>
#include <linux/time.h>
    
#include <asm/uaccess.h>

#include <mach/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mach/mt_pm_ldo.h>
#include <mach/eint.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_gpio.h>
#include <mach/mtk_rtc.h>
#include <mach/mt_spm_mtcmos.h>

#include <mach/battery_common.h>
#include <mach/pmic_mt6331_6332_sw.h>
#include <cust_pmic.h>
#include <cust_battery_meter.h>
//////////////////////////////////////////
// DVT enable
//////////////////////////////////////////
//BUCK
#define BUCK_ON_OFF_DVT_ENABLE
#define BUCK_VOSEL_DVT_ENABLE
#define BUCK_DLC_DVT_ENABLE
#define BUCK_BURST_DVT_ENABLE
//LDO
#define LDO_ON_OFF_DVT_ENABLE
#define LDO_VOSEL_DVT_ENABLE
#define LDO_CAL_DVT_ENABLE
#define LDO_MODE_DVT_ENABLE
//BIF
#define BIF_DVT_ENABLE
//TOP
#define TOP_DVT_ENABLE
//EFUSE
#define EFUSE_DVT_ENABLE


//AUXADC
#define AUXADC_DVT_ENABLE
//////////////////////////////////////////
// extern 
//////////////////////////////////////////
extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);
extern void upmu_set_reg_value(kal_uint32 reg, kal_uint32 reg_val);
extern U32 pmic_config_interface (U32 RegNum, U32 val, U32 MASK, U32 SHIFT);
extern int PMIC_IMM_GetOneChannelValue(upmu_adc_chl_list_enum dwChannel, int deCount, int trimd);
extern void dump_ldo_status_read_debug(void);
extern unsigned int get_pmic_mt6331_cid(void);
extern unsigned int get_pmic_mt6332_cid(void);

//////////////////////////////////////////
// BUCK
//////////////////////////////////////////
//MT6331
#define VDVFS11_INDEX 0
#define VDVFS12_INDEX 1
#define VDVFS13_INDEX 2
#define VDVFS14_INDEX 3
#define VGPU_INDEX    4
#define VCORE1_INDEX  5
#define VCORE2_INDEX  6
#define VIO18_INDEX   7
//MT6332
#define VDRAM_INDEX   8
#define VDVFS2_INDEX  9
#define VRF1_INDEX    10
#define VRF2_INDEX    11
#define VPA_INDEX     12
#define VSBST_INDEX   13
#define LDO_ONLY      99
#define ADC_ONLY      100

void read_adc_value(int index)
{
    int ret=1234;

    //mdelay(50);
    mdelay(500);
    
    #if 0
    ret = PMIC_IMM_GetOneChannelValue(ADC_ADCVIN0_AP,1,0);
    ret = (ret*4)/3; // from Ricky    
    #else
    ret = PMIC_IMM_GetOneChannelValue(ADC_VUSB_AP,1,0);    
    #endif

    printk("[read_auxadc_value] ret = %d\n", ret);

    //------------------------------------------------------------
    if(index == 0) {
        printk("mt6331_upmu_get_qi_vdvfs11_en=%d\n", mt6331_upmu_get_qi_vdvfs11_en());
        printk("mt6331_upmu_get_ni_vdvfs11_vosel=%d\n", mt6331_upmu_get_ni_vdvfs11_vosel());
    } 
    else if(index == 1) {
        printk("mt6331_upmu_get_qi_vdvfs12_en=%d\n", mt6331_upmu_get_qi_vdvfs12_en());
        printk("mt6331_upmu_get_ni_vdvfs12_vosel=%d\n", mt6331_upmu_get_ni_vdvfs12_vosel());
    }
    else if(index == 2) {
        printk("mt6331_upmu_get_qi_vdvfs13_en=%d\n", mt6331_upmu_get_qi_vdvfs13_en());
        printk("mt6331_upmu_get_ni_vdvfs13_vosel=%d\n", mt6331_upmu_get_ni_vdvfs13_vosel());
    }
    else if(index == 3) {
        printk("mt6331_upmu_get_qi_vdvfs14_en=%d\n", mt6331_upmu_get_qi_vdvfs14_en());
        printk("mt6331_upmu_get_ni_vdvfs14_vosel=%d\n", mt6331_upmu_get_ni_vdvfs14_vosel());
    }
    else if(index == 4) {
        printk("mt6331_upmu_get_qi_vgpu_en=%d\n", mt6331_upmu_get_qi_vgpu_en());
        printk("mt6331_upmu_get_ni_vgpu_vosel=%d\n", mt6331_upmu_get_ni_vgpu_vosel());
    }
    else if(index == 5) {
        printk("mt6331_upmu_get_qi_vcore1_en=%d\n", mt6331_upmu_get_qi_vcore1_en());
        printk("mt6331_upmu_get_ni_vcore1_vosel=%d\n", mt6331_upmu_get_ni_vcore1_vosel());
    }
    else if(index == 6) {
        printk("mt6331_upmu_get_qi_vcore2_en=%d\n", mt6331_upmu_get_qi_vcore2_en());
        printk("mt6331_upmu_get_ni_vcore2_vosel=%d\n", mt6331_upmu_get_ni_vcore2_vosel());
    }
    else if(index == 7) {
        printk("mt6331_upmu_get_qi_vio18_en=%d\n", mt6331_upmu_get_qi_vio18_en());
        printk("mt6331_upmu_get_ni_vio18_vosel=%d\n", mt6331_upmu_get_ni_vio18_vosel());
    }
    else if(index == 8) {
        printk("mt6332_upmu_get_qi_vdram_en=%d\n", mt6332_upmu_get_qi_vdram_en());
        printk("mt6332_upmu_get_ni_vdram_vosel=%d\n", mt6332_upmu_get_ni_vdram_vosel());
    }
    else if(index == 9) {
        printk("mt6332_upmu_get_qi_vdvfs2_en=%d\n", mt6332_upmu_get_qi_vdvfs2_en());
        printk("mt6332_upmu_get_ni_vdvfs2_vosel=%d\n", mt6332_upmu_get_ni_vdvfs2_vosel());
    }
    else if(index == 10) {
        printk("mt6332_upmu_get_qi_vrf1_en=%d\n", mt6332_upmu_get_qi_vrf1_en());
        printk("mt6332_upmu_get_ni_vrf1_vosel=%d\n", mt6332_upmu_get_ni_vrf1_vosel());
    }
    else if(index == 11) {
        printk("mt6332_upmu_get_qi_vrf2_en=%d\n", mt6332_upmu_get_qi_vrf2_en());
        printk("mt6332_upmu_get_ni_vrf2_vosel=%d\n", mt6332_upmu_get_ni_vrf2_vosel());
    }
    else if(index == 12) {
        printk("mt6332_upmu_get_qi_vpa_en=%d\n", mt6332_upmu_get_qi_vpa_en());
        printk("mt6332_upmu_get_ni_vpa_vosel=%d\n", mt6332_upmu_get_ni_vpa_vosel());
    }
    else if(index == 13) {
        printk("mt6332_upmu_get_qi_vsbst_en=%d\n", mt6332_upmu_get_qi_vsbst_en());
        printk("mt6332_upmu_get_ni_vsbst_vosel=%d\n", mt6332_upmu_get_ni_vsbst_vosel());
    }
    else if(index == 99) {
        dump_ldo_status_read_debug();    
    }
    else
    {        
    }

    printk("\n");
}

void set_srclken_sw_mode(void)
{
    //SRCLKEN SW mode
    //0:SW mode
    //1:HW mode
    
#if 1 // For 31 DVT
    mt6331_upmu_set_rg_srclken_in1_hw_mode(0);
    mt6331_upmu_set_rg_srclken_in2_hw_mode(0);
    printk("[31 SRCLKEN] Reg[0x%x]=0x%x, [5:4]\n", MT6331_TOP_CON, upmu_get_reg_value(MT6331_TOP_CON));
#else // For 32 DVT
    mt6332_upmu_set_rg_srclken_in1_hw_mode(0);
    mt6332_upmu_set_rg_srclken_in2_hw_mode(0);
    printk("[32 SRCLKEN] Reg[0x%x]=0x%x, [5:4]\n", MT6332_TOP_CON, upmu_get_reg_value(MT6332_TOP_CON));
#endif
}

void set_srclken_1_val(int val)
{
    //0:sleep mode
    //1:normal mode
    
#if 1 // For 31 DVT   
    #if 0 
    mt6331_upmu_set_rg_srclken_in1_en(val);
    printk("[31 SRCLKEN=%d] Reg[0x%x]=0x%x, [bit 0]\n", val, MT6331_TOP_CON, upmu_get_reg_value(MT6331_TOP_CON));
    #else //only for E1 
    mt6331_upmu_set_rg_srclken_in1_en(1);
    printk("[31 SRCLKEN=keep 1 on E1] Reg[0x%x]=0x%x, [bit 0]\n", MT6331_TOP_CON, upmu_get_reg_value(MT6331_TOP_CON));
    #endif
#else // For 32 DVT
    #if 0 
    mt6332_upmu_set_rg_srclken_in1_en(val);
    printk("[32 SRCLKEN=%d] Reg[0x%x]=0x%x, [bit 0]\n", val, MT6332_TOP_CON, upmu_get_reg_value(MT6332_TOP_CON));
    #else //only for E1 
    mt6332_upmu_set_rg_srclken_in1_en(1);
    printk("[32 SRCLKEN=keep 1 on E1] Reg[0x%x]=0x%x, [bit 0]\n", MT6332_TOP_CON, upmu_get_reg_value(MT6332_TOP_CON));
    #endif
#endif    
}

void set_srclken_2_val(int val)
{
    //0:sleep mode
    //1:normal mode
    
#if 1 // For 31 DVT   
    mt6331_upmu_set_rg_srclken_in2_en(val);
    printk("[31 SRCLKEN=%d] Reg[0x%x]=0x%x, [bit 1]\n", val, MT6331_TOP_CON, upmu_get_reg_value(MT6331_TOP_CON));
#else // For 32 DVT
    mt6332_upmu_set_rg_srclken_in2_en(val);
    printk("[32 SRCLKEN=%d] Reg[0x%x]=0x%x, [bit 1]\n", val, MT6332_TOP_CON, upmu_get_reg_value(MT6332_TOP_CON));
#endif    
}

void exec_scrxxx_map(int buck_index)
{
    set_srclken_sw_mode();

    printk("[exec_scrxxx_map] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); read_adc_value(buck_index);

    printk("[exec_scrxxx_map] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); read_adc_value(buck_index);

    printk("[exec_scrxxx_map] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); read_adc_value(buck_index);

    printk("[exec_scrxxx_map] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); read_adc_value(buck_index);    
}

#ifdef BUCK_ON_OFF_DVT_ENABLE
void exec_vdvfs11_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vdvfs11_en_ctrl] %d\n", i);
        mt6331_upmu_set_vdvfs11_en_ctrl(i);

        switch(i){
            case 0:
                printk("[mt6331_upmu_set_vdvfs11_en(0)]\n");
                mt6331_upmu_set_vdvfs11_en(0);
                read_adc_value(VDVFS11_INDEX);

                printk("[mt6331_upmu_set_vdvfs11_en(1)]\n");
                mt6331_upmu_set_vdvfs11_en(1);
                read_adc_value(VDVFS11_INDEX);
                break;

            case 1:
                printk("[mt6331_upmu_set_vdvfs11_en_sel(0)]\n");
                mt6331_upmu_set_vdvfs11_en_sel(0);
                exec_scrxxx_map(VDVFS11_INDEX);                       

                printk("[mt6331_upmu_set_vdvfs11_en_sel(1)]\n");
                mt6331_upmu_set_vdvfs11_en_sel(1);
                exec_scrxxx_map(VDVFS11_INDEX);

                printk("[mt6331_upmu_set_vdvfs11_en_sel(2)]\n");
                mt6331_upmu_set_vdvfs11_en_sel(2);
                exec_scrxxx_map(VDVFS11_INDEX);

                printk("[mt6331_upmu_set_vdvfs11_en_sel(3)]\n");
                mt6331_upmu_set_vdvfs11_en_sel(3);
                exec_scrxxx_map(VDVFS11_INDEX);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vdvfs12_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vdvfs12_en_ctrl] %d\n", i);
        mt6331_upmu_set_vdvfs12_en_ctrl(i);

        switch(i){
            case 0:
                printk("[mt6331_upmu_set_vdvfs12_en(0)]\n");
                mt6331_upmu_set_vdvfs12_en(0);
                read_adc_value(VDVFS12_INDEX);

                printk("[mt6331_upmu_set_vdvfs12_en(1)]\n");
                mt6331_upmu_set_vdvfs12_en(1);
                read_adc_value(VDVFS12_INDEX);
                break;

            case 1:
                printk("[mt6331_upmu_set_vdvfs12_en_sel(0)]\n");
                mt6331_upmu_set_vdvfs12_en_sel(0);
                exec_scrxxx_map(VDVFS12_INDEX);                       

                printk("[mt6331_upmu_set_vdvfs12_en_sel(1)]\n");
                mt6331_upmu_set_vdvfs12_en_sel(1);
                exec_scrxxx_map(VDVFS12_INDEX);

                printk("[mt6331_upmu_set_vdvfs12_en_sel(2)]\n");
                mt6331_upmu_set_vdvfs12_en_sel(2);
                exec_scrxxx_map(VDVFS12_INDEX);

                printk("[mt6331_upmu_set_vdvfs12_en_sel(3)]\n");
                mt6331_upmu_set_vdvfs12_en_sel(3);
                exec_scrxxx_map(VDVFS12_INDEX);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vdvfs13_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vdvfs13_en_ctrl] %d\n", i);
        mt6331_upmu_set_vdvfs13_en_ctrl(i);

        switch(i){
            case 0:
                printk("[mt6331_upmu_set_vdvfs13_en(0)]\n");
                mt6331_upmu_set_vdvfs13_en(0);
                read_adc_value(VDVFS13_INDEX);

                printk("[mt6331_upmu_set_vdvfs13_en(1)]\n");
                mt6331_upmu_set_vdvfs13_en(1);
                read_adc_value(VDVFS13_INDEX);
                break;

            case 1:
                printk("[mt6331_upmu_set_vdvfs13_en_sel(0)]\n");
                mt6331_upmu_set_vdvfs13_en_sel(0);
                exec_scrxxx_map(VDVFS13_INDEX);                       

                printk("[mt6331_upmu_set_vdvfs13_en_sel(1)]\n");
                mt6331_upmu_set_vdvfs13_en_sel(1);
                exec_scrxxx_map(VDVFS13_INDEX);

                printk("[mt6331_upmu_set_vdvfs13_en_sel(2)]\n");
                mt6331_upmu_set_vdvfs13_en_sel(2);
                exec_scrxxx_map(VDVFS13_INDEX);

                printk("[mt6331_upmu_set_vdvfs13_en_sel(3)]\n");
                mt6331_upmu_set_vdvfs13_en_sel(3);
                exec_scrxxx_map(VDVFS13_INDEX);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vdvfs14_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vdvfs14_en_ctrl] %d\n", i);
        mt6331_upmu_set_vdvfs14_en_ctrl(i);

        switch(i){
            case 0:
                printk("[mt6331_upmu_set_vdvfs14_en(0)]\n");
                mt6331_upmu_set_vdvfs14_en(0);
                read_adc_value(VDVFS14_INDEX);

                printk("[mt6331_upmu_set_vdvfs14_en(1)]\n");
                mt6331_upmu_set_vdvfs14_en(1);
                read_adc_value(VDVFS14_INDEX);
                break;

            case 1:
                printk("[mt6331_upmu_set_vdvfs14_en_sel(0)]\n");
                mt6331_upmu_set_vdvfs14_en_sel(0);
                exec_scrxxx_map(VDVFS14_INDEX);                       

                printk("[mt6331_upmu_set_vdvfs14_en_sel(1)]\n");
                mt6331_upmu_set_vdvfs14_en_sel(1);
                exec_scrxxx_map(VDVFS14_INDEX);

                printk("[mt6331_upmu_set_vdvfs14_en_sel(2)]\n");
                mt6331_upmu_set_vdvfs14_en_sel(2);
                exec_scrxxx_map(VDVFS14_INDEX);

                printk("[mt6331_upmu_set_vdvfs14_en_sel(3)]\n");
                mt6331_upmu_set_vdvfs14_en_sel(3);
                exec_scrxxx_map(VDVFS14_INDEX);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vgpu_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vgpu_en_ctrl] %d\n", i);
        mt6331_upmu_set_vgpu_en_ctrl(i);

        switch(i){
            case 0:
                printk("[mt6331_upmu_set_vgpu_en(0)]\n");
                mt6331_upmu_set_vgpu_en(0);
                read_adc_value(VGPU_INDEX);

                printk("[mt6331_upmu_set_vgpu_en(1)]\n");
                mt6331_upmu_set_vgpu_en(1);
                read_adc_value(VGPU_INDEX);
                break;

            case 1:
                printk("[mt6331_upmu_set_vgpu_en_sel(0)]\n");
                mt6331_upmu_set_vgpu_en_sel(0);
                exec_scrxxx_map(VGPU_INDEX);                       

                printk("[mt6331_upmu_set_vgpu_en_sel(1)]\n");
                mt6331_upmu_set_vgpu_en_sel(1);
                exec_scrxxx_map(VGPU_INDEX);

                printk("[mt6331_upmu_set_vgpu_en_sel(2)]\n");
                mt6331_upmu_set_vgpu_en_sel(2);
                exec_scrxxx_map(VGPU_INDEX);

                printk("[mt6331_upmu_set_vgpu_en_sel(3)]\n");
                mt6331_upmu_set_vgpu_en_sel(3);
                exec_scrxxx_map(VGPU_INDEX);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vcore1_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vcore1_en_ctrl] %d\n", i);
        mt6331_upmu_set_vcore1_en_ctrl(i);

        switch(i){
            case 0:
                printk("[mt6331_upmu_set_vcore1_en(0)]\n");
                mt6331_upmu_set_vcore1_en(0);
                read_adc_value(VCORE1_INDEX);

                printk("[mt6331_upmu_set_vcore1_en(1)]\n");
                mt6331_upmu_set_vcore1_en(1);
                read_adc_value(VCORE1_INDEX);
                break;

            case 1:
                printk("[mt6331_upmu_set_vcore1_en_sel(0)]\n");
                mt6331_upmu_set_vcore1_en_sel(0);
                exec_scrxxx_map(VCORE1_INDEX);                       

                printk("[mt6331_upmu_set_vcore1_en_sel(1)]\n");
                mt6331_upmu_set_vcore1_en_sel(1);
                exec_scrxxx_map(VCORE1_INDEX);

                printk("[mt6331_upmu_set_vcore1_en_sel(2)]\n");
                mt6331_upmu_set_vcore1_en_sel(2);
                exec_scrxxx_map(VCORE1_INDEX);

                printk("[mt6331_upmu_set_vcore1_en_sel(3)]\n");
                mt6331_upmu_set_vcore1_en_sel(3);
                exec_scrxxx_map(VCORE1_INDEX);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vcore2_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vcore2_en_ctrl] %d\n", i);
        mt6331_upmu_set_vcore2_en_ctrl(i);

        switch(i){
            case 0:
                printk("[mt6331_upmu_set_vcore2_en(0)]\n");
                mt6331_upmu_set_vcore2_en(0);
                read_adc_value(VCORE2_INDEX);

                printk("[mt6331_upmu_set_vcore2_en(1)]\n");
                mt6331_upmu_set_vcore2_en(1);
                read_adc_value(VCORE2_INDEX);
                break;

            case 1:
                printk("[mt6331_upmu_set_vcore2_en_sel(0)]\n");
                mt6331_upmu_set_vcore2_en_sel(0);
                exec_scrxxx_map(VCORE2_INDEX);                       

                printk("[mt6331_upmu_set_vcore2_en_sel(1)]\n");
                mt6331_upmu_set_vcore2_en_sel(1);
                exec_scrxxx_map(VCORE2_INDEX);

                printk("[mt6331_upmu_set_vcore2_en_sel(2)]\n");
                mt6331_upmu_set_vcore2_en_sel(2);
                exec_scrxxx_map(VCORE2_INDEX);

                printk("[mt6331_upmu_set_vcore2_en_sel(3)]\n");
                mt6331_upmu_set_vcore2_en_sel(3);
                exec_scrxxx_map(VCORE2_INDEX);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vio18_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vio18_en_ctrl] %d\n", i);
        mt6331_upmu_set_vio18_en_ctrl(i);

        switch(i){
            case 0:
                printk("[mt6331_upmu_set_vio18_en(0)]\n");
                mt6331_upmu_set_vio18_en(0);
                read_adc_value(VIO18_INDEX);

                printk("[mt6331_upmu_set_vio18_en(1)]\n");
                mt6331_upmu_set_vio18_en(1);
                read_adc_value(VIO18_INDEX);
                break;

            case 1:
                printk("[mt6331_upmu_set_vio18_en_sel(0)]\n");
                mt6331_upmu_set_vio18_en_sel(0);
                exec_scrxxx_map(VIO18_INDEX);                       

                printk("[mt6331_upmu_set_vio18_en_sel(1)]\n");
                mt6331_upmu_set_vio18_en_sel(1);
                exec_scrxxx_map(VIO18_INDEX);

                printk("[mt6331_upmu_set_vio18_en_sel(2)]\n");
                mt6331_upmu_set_vio18_en_sel(2);
                exec_scrxxx_map(VIO18_INDEX);

                printk("[mt6331_upmu_set_vio18_en_sel(3)]\n");
                mt6331_upmu_set_vio18_en_sel(3);
                exec_scrxxx_map(VIO18_INDEX);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vdram_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vdram_en_ctrl] %d\n", i);
        mt6332_upmu_set_vdram_en_ctrl(i);

        switch(i){
            case 0:
                printk("[mt6332_upmu_set_vdram_en(0)]\n");
                mt6332_upmu_set_vdram_en(0);
                read_adc_value(VDRAM_INDEX);

                printk("[mt6332_upmu_set_vdram_en(1)]\n");
                mt6332_upmu_set_vdram_en(1);
                read_adc_value(VDRAM_INDEX);
                break;

            case 1:
                printk("[mt6332_upmu_set_vdram_en_sel(0)]\n");
                mt6332_upmu_set_vdram_en_sel(0);
                exec_scrxxx_map(VDRAM_INDEX);                       

                printk("[mt6332_upmu_set_vdram_en_sel(1)]\n");
                mt6332_upmu_set_vdram_en_sel(1);
                exec_scrxxx_map(VDRAM_INDEX);

                printk("[mt6332_upmu_set_vdram_en_sel(2)]\n");
                mt6332_upmu_set_vdram_en_sel(2);
                exec_scrxxx_map(VDRAM_INDEX);

                printk("[mt6332_upmu_set_vdram_en_sel(3)]\n");
                mt6332_upmu_set_vdram_en_sel(3);
                exec_scrxxx_map(VDRAM_INDEX);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vdvfs2_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vdvfs2_en_ctrl] %d\n", i);
        mt6332_upmu_set_vdvfs2_en_ctrl(i);

        switch(i){
            case 0:
                printk("[mt6332_upmu_set_vdvfs2_en(0)]\n");
                mt6332_upmu_set_vdvfs2_en(0);
                read_adc_value(VDVFS2_INDEX);

                printk("[mt6332_upmu_set_vdvfs2_en(1)]\n");
                mt6332_upmu_set_vdvfs2_en(1);
                read_adc_value(VDVFS2_INDEX);
                break;

            case 1:
                printk("[mt6332_upmu_set_vdvfs2_en_sel(0)]\n");
                mt6332_upmu_set_vdvfs2_en_sel(0);
                exec_scrxxx_map(VDVFS2_INDEX);                       

                printk("[mt6332_upmu_set_vdvfs2_en_sel(1)]\n");
                mt6332_upmu_set_vdvfs2_en_sel(1);
                exec_scrxxx_map(VDVFS2_INDEX);

                printk("[mt6332_upmu_set_vdvfs2_en_sel(2)]\n");
                mt6332_upmu_set_vdvfs2_en_sel(2);
                exec_scrxxx_map(VDVFS2_INDEX);

                printk("[mt6332_upmu_set_vdvfs2_en_sel(3)]\n");
                mt6332_upmu_set_vdvfs2_en_sel(3);
                exec_scrxxx_map(VDVFS2_INDEX);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vrf1_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vrf1_en_ctrl] %d\n", i);
        mt6332_upmu_set_vrf1_en_ctrl(i);

        switch(i){
            case 0:
                printk("[mt6332_upmu_set_vrf1_en(0)]\n");
                mt6332_upmu_set_vrf1_en(0);
                read_adc_value(VRF1_INDEX);

                printk("[mt6332_upmu_set_vrf1_en(1)]\n");
                mt6332_upmu_set_vrf1_en(1);
                read_adc_value(VRF1_INDEX);
                break;

            case 1:
                printk("[mt6332_upmu_set_vrf1_en_sel(0)]\n");
                mt6332_upmu_set_vrf1_en_sel(0);
                exec_scrxxx_map(VRF1_INDEX);                       

                printk("[mt6332_upmu_set_vrf1_en_sel(1)]\n");
                mt6332_upmu_set_vrf1_en_sel(1);
                exec_scrxxx_map(VRF1_INDEX);

                printk("[mt6332_upmu_set_vrf1_en_sel(2)]\n");
                mt6332_upmu_set_vrf1_en_sel(2);
                exec_scrxxx_map(VRF1_INDEX);

                printk("[mt6332_upmu_set_vrf1_en_sel(3)]\n");
                mt6332_upmu_set_vrf1_en_sel(3);
                exec_scrxxx_map(VRF1_INDEX);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vrf2_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vrf2_en_ctrl] %d\n", i);
        mt6332_upmu_set_vrf2_en_ctrl(i);

        switch(i){
            case 0:
                printk("[mt6332_upmu_set_vrf2_en(0)]\n");
                mt6332_upmu_set_vrf2_en(0);
                read_adc_value(VRF2_INDEX);

                printk("[mt6332_upmu_set_vrf2_en(1)]\n");
                mt6332_upmu_set_vrf2_en(1);
                read_adc_value(VRF2_INDEX);
                break;

            case 1:
                printk("[mt6332_upmu_set_vrf2_en_sel(0)]\n");
                mt6332_upmu_set_vrf2_en_sel(0);
                exec_scrxxx_map(VRF2_INDEX);                       

                printk("[mt6332_upmu_set_vrf2_en_sel(1)]\n");
                mt6332_upmu_set_vrf2_en_sel(1);
                exec_scrxxx_map(VRF2_INDEX);

                printk("[mt6332_upmu_set_vrf2_en_sel(2)]\n");
                mt6332_upmu_set_vrf2_en_sel(2);
                exec_scrxxx_map(VRF2_INDEX);

                printk("[mt6332_upmu_set_vrf2_en_sel(3)]\n");
                mt6332_upmu_set_vrf2_en_sel(3);
                exec_scrxxx_map(VRF2_INDEX);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vpa_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vpa_en_ctrl] %d\n", i);
        mt6332_upmu_set_vpa_en_ctrl(i);

        switch(i){
            case 0:
                printk("[mt6332_upmu_set_vpa_en(0)]\n");
                mt6332_upmu_set_vpa_en(0);
                read_adc_value(VPA_INDEX);

                printk("[mt6332_upmu_set_vpa_en(1)]\n");
                mt6332_upmu_set_vpa_en(1);
                read_adc_value(VPA_INDEX);
                break;

            case 1:
                printk("[mt6332_upmu_set_vpa_en_sel(0)]\n");
                mt6332_upmu_set_vpa_en_sel(0);
                exec_scrxxx_map(VPA_INDEX);                       

                printk("[mt6332_upmu_set_vpa_en_sel(1)]\n");
                mt6332_upmu_set_vpa_en_sel(1);
                exec_scrxxx_map(VPA_INDEX);

                printk("[mt6332_upmu_set_vpa_en_sel(2)]\n");
                mt6332_upmu_set_vpa_en_sel(2);
                exec_scrxxx_map(VPA_INDEX);

                printk("[mt6332_upmu_set_vpa_en_sel(3)]\n");
                mt6332_upmu_set_vpa_en_sel(3);
                exec_scrxxx_map(VPA_INDEX);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vsbst_en_test(void)
{
    int i=0;

    printk("[exec_vsbst_en_test] mt6332_upmu_set_rg_vsbst_3m_ck_pdn(0);\n");
    mt6332_upmu_set_rg_vsbst_3m_ck_pdn(0);    

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vsbst_en_ctrl] %d\n", i);
        mt6332_upmu_set_vsbst_en_ctrl(i);

        switch(i){
            case 0:
                printk("[mt6332_upmu_set_vsbst_en(0)]\n");
                mt6332_upmu_set_vsbst_en(0);
                read_adc_value(VSBST_INDEX);

                printk("[mt6332_upmu_set_vsbst_en(1)]\n");
                mt6332_upmu_set_vsbst_en(1);
                read_adc_value(VSBST_INDEX);
                break;

            case 1:
                printk("[mt6332_upmu_set_vsbst_en_sel(0)]\n");
                mt6332_upmu_set_vsbst_en_sel(0);
                exec_scrxxx_map(VSBST_INDEX);                       

                printk("[mt6332_upmu_set_vsbst_en_sel(1)]\n");
                mt6332_upmu_set_vsbst_en_sel(1);
                exec_scrxxx_map(VSBST_INDEX);

                printk("[mt6332_upmu_set_vsbst_en_sel(2)]\n");
                mt6332_upmu_set_vsbst_en_sel(2);
                exec_scrxxx_map(VSBST_INDEX);

                printk("[mt6332_upmu_set_vsbst_en_sel(3)]\n");
                mt6332_upmu_set_vsbst_en_sel(3);
                exec_scrxxx_map(VSBST_INDEX);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void PMIC_BUCK_ON_OFF(int index_val)
{
    printk("[PMIC_BUCK_ON_OFF] start....\n");
    
    set_srclken_sw_mode();

    switch(index_val){
      //MT6331
      case 0 : exec_vdvfs11_en_test();     break;
      case 1 : exec_vdvfs12_en_test();     break;
      case 2 : exec_vdvfs13_en_test();     break;
      case 3 : exec_vdvfs14_en_test();     break;
      case 4 : exec_vgpu_en_test();        break;
      case 5 : exec_vcore1_en_test();      break;
      case 6 : exec_vcore2_en_test();      break;
      case 7 : exec_vio18_en_test();       break;

      //MT6331
      case 8 : exec_vdram_en_test();       break;
      case 9 : exec_vdvfs2_en_test();      break;
      case 10: exec_vrf1_en_test();        break;
      case 11: exec_vrf2_en_test();        break;
      case 12: exec_vpa_en_test();         break;      
      case 13: exec_vsbst_en_test();       break;

      default:
        printk("[PMIC_BUCK_ON_OFF] Invalid channel value(%d)\n", index_val);
        break;

    }

    printk("[PMIC_BUCK_ON_OFF] end....\n");
}
#endif

#ifdef BUCK_VOSEL_DVT_ENABLE
void exec_vdvfs11_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VDVFS11_VOSEL_SLEEP_MASK;k++) {
        mt6331_upmu_set_vdvfs11_vosel_sleep(k); printk("[mt6331_upmu_set_vdvfs11_vosel_sleep] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VDVFS11_INDEX);
    }
    for(k=0;k<=MT6331_PMIC_VDVFS11_VOSEL_ON_MASK;k++) {
        mt6331_upmu_set_vdvfs11_vosel_on(k); printk("[mt6331_upmu_set_vdvfs11_vosel_on] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VDVFS11_INDEX);
    }
}

void exec_vdvfs11_vosel_test_sub(void)
{
    printk("[exec_vdvfs11_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vdvfs11_vosel_test_sub_vosel();

    printk("[exec_vdvfs11_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vdvfs11_vosel_test_sub_vosel();

    printk("[exec_vdvfs11_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vdvfs11_vosel_test_sub_vosel();

    printk("[exec_vdvfs11_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vdvfs11_vosel_test_sub_vosel();
}

void exec_vdvfs11_vosel_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vdvfs11_vosel_ctrl] %d\n", i);
        mt6331_upmu_set_vdvfs11_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VDVFS11_VOSEL_MASK;k++) {
                    mt6331_upmu_set_vdvfs11_vosel(k); printk("[mt6331_upmu_set_vdvfs11_vosel] k=%d, ",k);
                    if(k==0) mdelay(500);
                    read_adc_value(VDVFS11_INDEX);
                }
                break;

            case 1:
                printk("[mt6331_upmu_set_vdvfs11_vosel_sel(0)]\n");
                mt6331_upmu_set_vdvfs11_vosel_sel(0);
                exec_vdvfs11_vosel_test_sub(); 

                printk("[mt6331_upmu_set_vdvfs11_vosel_sel(1)]\n");
                mt6331_upmu_set_vdvfs11_vosel_sel(1);                
                exec_vdvfs11_vosel_test_sub();

                printk("[mt6331_upmu_set_vdvfs11_vosel_sel(2)]\n");
                mt6331_upmu_set_vdvfs11_vosel_sel(2);
                exec_vdvfs11_vosel_test_sub();

                printk("[mt6331_upmu_set_vdvfs11_vosel_sel(3)]\n");
                mt6331_upmu_set_vdvfs11_vosel_sel(3);
                exec_vdvfs11_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vdvfs12_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VDVFS12_VOSEL_SLEEP_MASK;k++) {
        mt6331_upmu_set_vdvfs12_vosel_sleep(k); printk("[mt6331_upmu_set_vdvfs12_vosel_sleep] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VDVFS12_INDEX);
    }
    for(k=0;k<=MT6331_PMIC_VDVFS12_VOSEL_ON_MASK;k++) {
        mt6331_upmu_set_vdvfs12_vosel_on(k); printk("[mt6331_upmu_set_vdvfs12_vosel_on] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VDVFS12_INDEX);
    }
}

void exec_vdvfs12_vosel_test_sub(void)
{
    printk("[exec_vdvfs12_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vdvfs12_vosel_test_sub_vosel();

    printk("[exec_vdvfs12_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vdvfs12_vosel_test_sub_vosel();

    printk("[exec_vdvfs12_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vdvfs12_vosel_test_sub_vosel();

    printk("[exec_vdvfs12_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vdvfs12_vosel_test_sub_vosel();
}

void exec_vdvfs12_vosel_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vdvfs12_vosel_ctrl] %d\n", i);
        mt6331_upmu_set_vdvfs12_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VDVFS12_VOSEL_MASK;k++) {
                    mt6331_upmu_set_vdvfs12_vosel(k); printk("[mt6331_upmu_set_vdvfs12_vosel] k=%d, ",k);
                    if(k==0) mdelay(500);
                    read_adc_value(VDVFS12_INDEX);
                }
                break;

            case 1:
                printk("[mt6331_upmu_set_vdvfs12_vosel_sel(0)]\n");
                mt6331_upmu_set_vdvfs12_vosel_sel(0);
                exec_vdvfs12_vosel_test_sub(); 

                printk("[mt6331_upmu_set_vdvfs12_vosel_sel(1)]\n");
                mt6331_upmu_set_vdvfs12_vosel_sel(1);                
                exec_vdvfs12_vosel_test_sub();

                printk("[mt6331_upmu_set_vdvfs12_vosel_sel(2)]\n");
                mt6331_upmu_set_vdvfs12_vosel_sel(2);
                exec_vdvfs12_vosel_test_sub();

                printk("[mt6331_upmu_set_vdvfs12_vosel_sel(3)]\n");
                mt6331_upmu_set_vdvfs12_vosel_sel(3);
                exec_vdvfs12_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vdvfs13_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VDVFS13_VOSEL_SLEEP_MASK;k++) {
        mt6331_upmu_set_vdvfs13_vosel_sleep(k); printk("[mt6331_upmu_set_vdvfs13_vosel_sleep] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VDVFS13_INDEX);
    }
    for(k=0;k<=MT6331_PMIC_VDVFS13_VOSEL_ON_MASK;k++) {
        mt6331_upmu_set_vdvfs13_vosel_on(k); printk("[mt6331_upmu_set_vdvfs13_vosel_on] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VDVFS13_INDEX);
    }
}

void exec_vdvfs13_vosel_test_sub(void)
{
    printk("[exec_vdvfs13_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vdvfs13_vosel_test_sub_vosel();

    printk("[exec_vdvfs13_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vdvfs13_vosel_test_sub_vosel();

    printk("[exec_vdvfs13_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vdvfs13_vosel_test_sub_vosel();

    printk("[exec_vdvfs13_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vdvfs13_vosel_test_sub_vosel();
}

void exec_vdvfs13_vosel_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vdvfs13_vosel_ctrl] %d\n", i);
        mt6331_upmu_set_vdvfs13_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VDVFS13_VOSEL_MASK;k++) {
                    mt6331_upmu_set_vdvfs13_vosel(k); printk("[mt6331_upmu_set_vdvfs13_vosel] k=%d, ",k);
                    if(k==0) mdelay(500);
                    read_adc_value(VDVFS13_INDEX);
                }
                break;

            case 1:
                printk("[mt6331_upmu_set_vdvfs13_vosel_sel(0)]\n");
                mt6331_upmu_set_vdvfs13_vosel_sel(0);
                exec_vdvfs13_vosel_test_sub(); 

                printk("[mt6331_upmu_set_vdvfs13_vosel_sel(1)]\n");
                mt6331_upmu_set_vdvfs13_vosel_sel(1);                
                exec_vdvfs13_vosel_test_sub();

                printk("[mt6331_upmu_set_vdvfs13_vosel_sel(2)]\n");
                mt6331_upmu_set_vdvfs13_vosel_sel(2);
                exec_vdvfs13_vosel_test_sub();

                printk("[mt6331_upmu_set_vdvfs13_vosel_sel(3)]\n");
                mt6331_upmu_set_vdvfs13_vosel_sel(3);
                exec_vdvfs13_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vdvfs14_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VDVFS14_VOSEL_SLEEP_MASK;k++) {
        mt6331_upmu_set_vdvfs14_vosel_sleep(k); printk("[mt6331_upmu_set_vdvfs14_vosel_sleep] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VDVFS14_INDEX);
    }
    for(k=0;k<=MT6331_PMIC_VDVFS14_VOSEL_ON_MASK;k++) {
        mt6331_upmu_set_vdvfs14_vosel_on(k); printk("[mt6331_upmu_set_vdvfs14_vosel_on] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VDVFS14_INDEX);
    }
}

void exec_vdvfs14_vosel_test_sub(void)
{
    printk("[exec_vdvfs14_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vdvfs14_vosel_test_sub_vosel();

    printk("[exec_vdvfs14_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vdvfs14_vosel_test_sub_vosel();

    printk("[exec_vdvfs14_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vdvfs14_vosel_test_sub_vosel();

    printk("[exec_vdvfs14_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vdvfs14_vosel_test_sub_vosel();
}

void exec_vdvfs14_vosel_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vdvfs14_vosel_ctrl] %d\n", i);
        mt6331_upmu_set_vdvfs14_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VDVFS14_VOSEL_MASK;k++) {
                    mt6331_upmu_set_vdvfs14_vosel(k); printk("[mt6331_upmu_set_vdvfs14_vosel] k=%d, ",k);
                    if(k==0) mdelay(500);
                    read_adc_value(VDVFS14_INDEX);
                }
                break;

            case 1:
                printk("[mt6331_upmu_set_vdvfs14_vosel_sel(0)]\n");
                mt6331_upmu_set_vdvfs14_vosel_sel(0);
                exec_vdvfs14_vosel_test_sub(); 

                printk("[mt6331_upmu_set_vdvfs14_vosel_sel(1)]\n");
                mt6331_upmu_set_vdvfs14_vosel_sel(1);                
                exec_vdvfs14_vosel_test_sub();

                printk("[mt6331_upmu_set_vdvfs14_vosel_sel(2)]\n");
                mt6331_upmu_set_vdvfs14_vosel_sel(2);
                exec_vdvfs14_vosel_test_sub();

                printk("[mt6331_upmu_set_vdvfs14_vosel_sel(3)]\n");
                mt6331_upmu_set_vdvfs14_vosel_sel(3);
                exec_vdvfs14_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vcore1_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VCORE1_VOSEL_SLEEP_MASK;k++) {
        mt6331_upmu_set_vcore1_vosel_sleep(k); printk("[mt6331_upmu_set_vcore1_vosel_sleep] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VCORE1_INDEX);
    }
    for(k=0;k<=MT6331_PMIC_VCORE1_VOSEL_ON_MASK;k++) {
        mt6331_upmu_set_vcore1_vosel_on(k); printk("[mt6331_upmu_set_vcore1_vosel_on] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VCORE1_INDEX);
    }
}

void exec_vcore1_vosel_test_sub(void)
{
    printk("[exec_vcore1_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vcore1_vosel_test_sub_vosel();

    printk("[exec_vcore1_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vcore1_vosel_test_sub_vosel();

    printk("[exec_vcore1_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vcore1_vosel_test_sub_vosel();

    printk("[exec_vcore1_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vcore1_vosel_test_sub_vosel();
}

void exec_vcore1_vosel_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vcore1_vosel_ctrl] %d\n", i);
        mt6331_upmu_set_vcore1_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VCORE1_VOSEL_MASK;k++) {
                    mt6331_upmu_set_vcore1_vosel(k); printk("[mt6331_upmu_set_vcore1_vosel] k=%d, ",k);
                    if(k==0) mdelay(500);
                    read_adc_value(VCORE1_INDEX);
                }
                break;

            case 1:
                printk("[mt6331_upmu_set_vcore1_vosel_sel(0)]\n");
                mt6331_upmu_set_vcore1_vosel_sel(0);
                exec_vcore1_vosel_test_sub(); 

                printk("[mt6331_upmu_set_vcore1_vosel_sel(1)]\n");
                mt6331_upmu_set_vcore1_vosel_sel(1);                
                exec_vcore1_vosel_test_sub();

                printk("[mt6331_upmu_set_vcore1_vosel_sel(2)]\n");
                mt6331_upmu_set_vcore1_vosel_sel(2);
                exec_vcore1_vosel_test_sub();

                printk("[mt6331_upmu_set_vcore1_vosel_sel(3)]\n");
                mt6331_upmu_set_vcore1_vosel_sel(3);
                exec_vcore1_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vcore2_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VCORE2_VOSEL_SLEEP_MASK;k++) {
        mt6331_upmu_set_vcore2_vosel_sleep(k); printk("[mt6331_upmu_set_vcore2_vosel_sleep] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VCORE2_INDEX);
    }
    for(k=0;k<=MT6331_PMIC_VCORE2_VOSEL_ON_MASK;k++) {
        mt6331_upmu_set_vcore2_vosel_on(k); printk("[mt6331_upmu_set_vcore2_vosel_on] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VCORE2_INDEX);
    }
}

void exec_vcore2_vosel_test_sub(void)
{
    printk("[exec_vcore2_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vcore2_vosel_test_sub_vosel();

    printk("[exec_vcore2_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vcore2_vosel_test_sub_vosel();

    printk("[exec_vcore2_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vcore2_vosel_test_sub_vosel();

    printk("[exec_vcore2_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vcore2_vosel_test_sub_vosel();
}

void exec_vcore2_vosel_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vcore2_vosel_ctrl] %d\n", i);
        mt6331_upmu_set_vcore2_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VCORE2_VOSEL_MASK;k++) {
                    mt6331_upmu_set_vcore2_vosel(k); printk("[mt6331_upmu_set_vcore2_vosel] k=%d, ",k);
                    if(k==0) mdelay(500);
                    read_adc_value(VCORE2_INDEX);
                }
                break;

            case 1:
                printk("[mt6331_upmu_set_vcore2_vosel_sel(0)]\n");
                mt6331_upmu_set_vcore2_vosel_sel(0);
                exec_vcore2_vosel_test_sub(); 

                printk("[mt6331_upmu_set_vcore2_vosel_sel(1)]\n");
                mt6331_upmu_set_vcore2_vosel_sel(1);                
                exec_vcore2_vosel_test_sub();

                printk("[mt6331_upmu_set_vcore2_vosel_sel(2)]\n");
                mt6331_upmu_set_vcore2_vosel_sel(2);
                exec_vcore2_vosel_test_sub();

                printk("[mt6331_upmu_set_vcore2_vosel_sel(3)]\n");
                mt6331_upmu_set_vcore2_vosel_sel(3);
                exec_vcore2_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vgpu_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VGPU_VOSEL_SLEEP_MASK;k++) {
        mt6331_upmu_set_vgpu_vosel_sleep(k); printk("[mt6331_upmu_set_vgpu_vosel_sleep] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VGPU_INDEX);
    }
    for(k=0;k<=MT6331_PMIC_VGPU_VOSEL_ON_MASK;k++) {
        mt6331_upmu_set_vgpu_vosel_on(k); printk("[mt6331_upmu_set_vgpu_vosel_on] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VGPU_INDEX);
    }
}

void exec_vgpu_vosel_test_sub(void)
{
    printk("[exec_vgpu_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vgpu_vosel_test_sub_vosel();

    printk("[exec_vgpu_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vgpu_vosel_test_sub_vosel();

    printk("[exec_vgpu_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vgpu_vosel_test_sub_vosel();

    printk("[exec_vgpu_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vgpu_vosel_test_sub_vosel();
}

void exec_vgpu_vosel_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vgpu_vosel_ctrl] %d\n", i);
        mt6331_upmu_set_vgpu_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VGPU_VOSEL_MASK;k++) {
                    mt6331_upmu_set_vgpu_vosel(k); printk("[mt6331_upmu_set_vgpu_vosel] k=%d, ",k);
                    if(k==0) mdelay(500);
                    read_adc_value(VGPU_INDEX);
                }
                break;

            case 1:
                printk("[mt6331_upmu_set_vgpu_vosel_sel(0)]\n");
                mt6331_upmu_set_vgpu_vosel_sel(0);
                exec_vgpu_vosel_test_sub(); 

                printk("[mt6331_upmu_set_vgpu_vosel_sel(1)]\n");
                mt6331_upmu_set_vgpu_vosel_sel(1);                
                exec_vgpu_vosel_test_sub();

                printk("[mt6331_upmu_set_vgpu_vosel_sel(2)]\n");
                mt6331_upmu_set_vgpu_vosel_sel(2);
                exec_vgpu_vosel_test_sub();

                printk("[mt6331_upmu_set_vgpu_vosel_sel(3)]\n");
                mt6331_upmu_set_vgpu_vosel_sel(3);
                exec_vgpu_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vio18_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VIO18_VOSEL_SLEEP_MASK;k++) {
        mt6331_upmu_set_vio18_vosel_sleep(k); printk("[mt6331_upmu_set_vio18_vosel_sleep] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VIO18_INDEX);
    }
    for(k=0;k<=MT6331_PMIC_VIO18_VOSEL_ON_MASK;k++) {
        mt6331_upmu_set_vio18_vosel_on(k); printk("[mt6331_upmu_set_vio18_vosel_on] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VIO18_INDEX);
    }
}

void exec_vio18_vosel_test_sub(void)
{
    printk("[exec_vio18_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vio18_vosel_test_sub_vosel();

    printk("[exec_vio18_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vio18_vosel_test_sub_vosel();

    printk("[exec_vio18_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vio18_vosel_test_sub_vosel();

    printk("[exec_vio18_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vio18_vosel_test_sub_vosel();
}

void exec_vio18_vosel_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vio18_vosel_ctrl] %d\n", i);
        mt6331_upmu_set_vio18_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VIO18_VOSEL_MASK;k++) {
                    mt6331_upmu_set_vio18_vosel(k); printk("[mt6331_upmu_set_vio18_vosel] k=%d, ",k);
                    if(k==0) mdelay(500);
                    read_adc_value(VIO18_INDEX);
                }
                break;

            case 1:
                printk("[mt6331_upmu_set_vio18_vosel_sel(0)]\n");
                mt6331_upmu_set_vio18_vosel_sel(0);
                exec_vio18_vosel_test_sub(); 

                printk("[mt6331_upmu_set_vio18_vosel_sel(1)]\n");
                mt6331_upmu_set_vio18_vosel_sel(1);                
                exec_vio18_vosel_test_sub();

                printk("[mt6331_upmu_set_vio18_vosel_sel(2)]\n");
                mt6331_upmu_set_vio18_vosel_sel(2);
                exec_vio18_vosel_test_sub();

                printk("[mt6331_upmu_set_vio18_vosel_sel(3)]\n");
                mt6331_upmu_set_vio18_vosel_sel(3);
                exec_vio18_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vdram_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VDRAM_VOSEL_SLEEP_MASK;k++) {
        mt6332_upmu_set_vdram_vosel_sleep(k); printk("[mt6332_upmu_set_vdram_vosel_sleep] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VDRAM_INDEX);
    }
    for(k=0;k<=MT6332_PMIC_VDRAM_VOSEL_ON_MASK;k++) {
        mt6332_upmu_set_vdram_vosel_on(k); printk("[mt6332_upmu_set_vdram_vosel_on] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VDRAM_INDEX);
    }
}

void exec_vdram_vosel_test_sub(void)
{
    printk("[exec_vdram_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vdram_vosel_test_sub_vosel();

    printk("[exec_vdram_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vdram_vosel_test_sub_vosel();

    printk("[exec_vdram_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vdram_vosel_test_sub_vosel();

    printk("[exec_vdram_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vdram_vosel_test_sub_vosel();
}

void exec_vdram_vosel_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vdram_vosel_ctrl] %d\n", i);
        mt6332_upmu_set_vdram_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VDRAM_VOSEL_MASK;k++) {
                    mt6332_upmu_set_vdram_vosel(k); printk("[mt6332_upmu_set_vdram_vosel] k=%d, ",k);
                    if(k==0) mdelay(500);
                    read_adc_value(VDRAM_INDEX);
                }
                break;

            case 1:
                printk("[mt6332_upmu_set_vdram_vosel_sel(0)]\n");
                mt6332_upmu_set_vdram_vosel_sel(0);
                exec_vdram_vosel_test_sub(); 

                printk("[mt6332_upmu_set_vdram_vosel_sel(1)]\n");
                mt6332_upmu_set_vdram_vosel_sel(1);                
                exec_vdram_vosel_test_sub();

                printk("[mt6332_upmu_set_vdram_vosel_sel(2)]\n");
                mt6332_upmu_set_vdram_vosel_sel(2);
                exec_vdram_vosel_test_sub();

                printk("[mt6332_upmu_set_vdram_vosel_sel(3)]\n");
                mt6332_upmu_set_vdram_vosel_sel(3);
                exec_vdram_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vdvfs2_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VDVFS2_VOSEL_SLEEP_MASK;k++) {
        mt6332_upmu_set_vdvfs2_vosel_sleep(k); printk("[mt6332_upmu_set_vdvfs2_vosel_sleep] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VDVFS2_INDEX);
    }

    printk("[exec_vdvfs2_vosel_test_sub_vosel] mt6332_upmu_set_vdvfs2_track_on_ctrl(0);\n"); mt6332_upmu_set_vdvfs2_track_on_ctrl(0);    

    for(k=0;k<=MT6332_PMIC_VDVFS2_VOSEL_ON_MASK;k++) {
        mt6332_upmu_set_vdvfs2_vosel_on(k); printk("[mt6332_upmu_set_vdvfs2_vosel_on] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VDVFS2_INDEX);
    }    
}

void exec_vdvfs2_vosel_test_sub(void)
{
    printk("[exec_vdvfs2_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vdvfs2_vosel_test_sub_vosel();

    printk("[exec_vdvfs2_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vdvfs2_vosel_test_sub_vosel();

    printk("[exec_vdvfs2_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vdvfs2_vosel_test_sub_vosel();

    printk("[exec_vdvfs2_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vdvfs2_vosel_test_sub_vosel();
}

void exec_vdvfs2_vosel_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vdvfs2_vosel_ctrl] %d\n", i);
        mt6332_upmu_set_vdvfs2_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VDVFS2_VOSEL_MASK;k++) {
                    mt6332_upmu_set_vdvfs2_vosel(k); printk("[mt6332_upmu_set_vdvfs2_vosel] k=%d, ",k);
                    if(k==0) mdelay(500);
                    read_adc_value(VDVFS2_INDEX);
                }
                break;

            case 1:
                printk("[mt6332_upmu_set_vdvfs2_vosel_sel(0)]\n");
                mt6332_upmu_set_vdvfs2_vosel_sel(0);
                exec_vdvfs2_vosel_test_sub(); 

                printk("[mt6332_upmu_set_vdvfs2_vosel_sel(1)]\n");
                mt6332_upmu_set_vdvfs2_vosel_sel(1);                
                exec_vdvfs2_vosel_test_sub();

                printk("[mt6332_upmu_set_vdvfs2_vosel_sel(2)]\n");
                mt6332_upmu_set_vdvfs2_vosel_sel(2);
                exec_vdvfs2_vosel_test_sub();

                printk("[mt6332_upmu_set_vdvfs2_vosel_sel(3)]\n");
                mt6332_upmu_set_vdvfs2_vosel_sel(3);
                exec_vdvfs2_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}


void exec_vrf1_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VRF1_VOSEL_SLEEP_MASK;k++) {
        mt6332_upmu_set_vrf1_vosel_sleep(k); printk("[mt6332_upmu_set_vrf1_vosel_sleep] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VRF1_INDEX);
    }
    for(k=0;k<=MT6332_PMIC_VRF1_VOSEL_ON_MASK;k++) {
        mt6332_upmu_set_vrf1_vosel_on(k); printk("[mt6332_upmu_set_vrf1_vosel_on] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VRF1_INDEX);
    }
}

void exec_vrf1_vosel_test_sub(void)
{
    printk("[exec_vrf1_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vrf1_vosel_test_sub_vosel();

    printk("[exec_vrf1_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vrf1_vosel_test_sub_vosel();

    printk("[exec_vrf1_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vrf1_vosel_test_sub_vosel();

    printk("[exec_vrf1_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vrf1_vosel_test_sub_vosel();
}

void exec_vrf1_vosel_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vrf1_vosel_ctrl] %d\n", i);
        mt6332_upmu_set_vrf1_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VRF1_VOSEL_MASK;k++) {
                    mt6332_upmu_set_vrf1_vosel(k); printk("[mt6332_upmu_set_vrf1_vosel] k=%d, ",k);
                    if(k==0) mdelay(500);
                    read_adc_value(VRF1_INDEX);
                }
                break;

            case 1:
                printk("[mt6332_upmu_set_vrf1_vosel_sel(0)]\n");
                mt6332_upmu_set_vrf1_vosel_sel(0);
                exec_vrf1_vosel_test_sub(); 

                printk("[mt6332_upmu_set_vrf1_vosel_sel(1)]\n");
                mt6332_upmu_set_vrf1_vosel_sel(1);                
                exec_vrf1_vosel_test_sub();

                printk("[mt6332_upmu_set_vrf1_vosel_sel(2)]\n");
                mt6332_upmu_set_vrf1_vosel_sel(2);
                exec_vrf1_vosel_test_sub();

                printk("[mt6332_upmu_set_vrf1_vosel_sel(3)]\n");
                mt6332_upmu_set_vrf1_vosel_sel(3);
                exec_vrf1_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vrf2_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VRF2_VOSEL_SLEEP_MASK;k++) {
        mt6332_upmu_set_vrf2_vosel_sleep(k); printk("[mt6332_upmu_set_vrf2_vosel_sleep] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VRF2_INDEX);
    }
    for(k=0;k<=MT6332_PMIC_VRF2_VOSEL_ON_MASK;k++) {
        mt6332_upmu_set_vrf2_vosel_on(k); printk("[mt6332_upmu_set_vrf2_vosel_on] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VRF2_INDEX);
    }
}

void exec_vrf2_vosel_test_sub(void)
{
    printk("[exec_vrf2_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vrf2_vosel_test_sub_vosel();

    printk("[exec_vrf2_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vrf2_vosel_test_sub_vosel();

    printk("[exec_vrf2_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vrf2_vosel_test_sub_vosel();

    printk("[exec_vrf2_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vrf2_vosel_test_sub_vosel();
}

void exec_vrf2_vosel_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vrf2_vosel_ctrl] %d\n", i);
        mt6332_upmu_set_vrf2_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VRF2_VOSEL_MASK;k++) {
                    mt6332_upmu_set_vrf2_vosel(k); printk("[mt6332_upmu_set_vrf2_vosel] k=%d, ",k);
                    if(k==0) mdelay(500);
                    read_adc_value(VRF2_INDEX);
                }
                break;

            case 1:
                printk("[mt6332_upmu_set_vrf2_vosel_sel(0)]\n");
                mt6332_upmu_set_vrf2_vosel_sel(0);
                exec_vrf2_vosel_test_sub(); 

                printk("[mt6332_upmu_set_vrf2_vosel_sel(1)]\n");
                mt6332_upmu_set_vrf2_vosel_sel(1);                
                exec_vrf2_vosel_test_sub();

                printk("[mt6332_upmu_set_vrf2_vosel_sel(2)]\n");
                mt6332_upmu_set_vrf2_vosel_sel(2);
                exec_vrf2_vosel_test_sub();

                printk("[mt6332_upmu_set_vrf2_vosel_sel(3)]\n");
                mt6332_upmu_set_vrf2_vosel_sel(3);
                exec_vrf2_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vpa_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VPA_VOSEL_SLEEP_MASK;k++) {
        mt6332_upmu_set_vpa_vosel_sleep(k); printk("[mt6332_upmu_set_vpa_vosel_sleep] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VPA_INDEX);
    }
    for(k=0;k<=MT6332_PMIC_VPA_VOSEL_ON_MASK;k++) {
        mt6332_upmu_set_vpa_vosel_on(k); printk("[mt6332_upmu_set_vpa_vosel_on] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VPA_INDEX);
    }
}

void exec_vpa_vosel_test_sub(void)
{
    printk("[exec_vpa_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vpa_vosel_test_sub_vosel();

    printk("[exec_vpa_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vpa_vosel_test_sub_vosel();

    printk("[exec_vpa_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vpa_vosel_test_sub_vosel();

    printk("[exec_vpa_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vpa_vosel_test_sub_vosel();
}

void exec_vpa_vosel_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vpa_vosel_ctrl] %d\n", i);
        mt6332_upmu_set_vpa_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VPA_VOSEL_MASK;k++) {
                    mt6332_upmu_set_vpa_vosel(k); printk("[mt6332_upmu_set_vpa_vosel] k=%d, ",k);
                    if(k==0) mdelay(500);
                    read_adc_value(VPA_INDEX);
                }
                break;

            case 1:
                printk("[mt6332_upmu_set_vpa_vosel_sel(0)]\n");
                mt6332_upmu_set_vpa_vosel_sel(0);
                exec_vpa_vosel_test_sub(); 

                printk("[mt6332_upmu_set_vpa_vosel_sel(1)]\n");
                mt6332_upmu_set_vpa_vosel_sel(1);                
                exec_vpa_vosel_test_sub();

                printk("[mt6332_upmu_set_vpa_vosel_sel(2)]\n");
                mt6332_upmu_set_vpa_vosel_sel(2);
                exec_vpa_vosel_test_sub();

                printk("[mt6332_upmu_set_vpa_vosel_sel(3)]\n");
                mt6332_upmu_set_vpa_vosel_sel(3);
                exec_vpa_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vsbst_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VSBST_VOSEL_SLEEP_MASK;k++) {
        mt6332_upmu_set_vsbst_vosel_sleep(k); printk("[mt6332_upmu_set_vsbst_vosel_sleep] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VSBST_INDEX);
    }
    for(k=0;k<=MT6332_PMIC_VSBST_VOSEL_ON_MASK;k++) {
        mt6332_upmu_set_vsbst_vosel_on(k); printk("[mt6332_upmu_set_vsbst_vosel_on] k=%d, ",k);
        if(k==0) mdelay(500);
        read_adc_value(VSBST_INDEX);
    }
}

void exec_vsbst_vosel_test_sub(void)
{
    printk("[exec_vsbst_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vsbst_vosel_test_sub_vosel();

    printk("[exec_vsbst_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vsbst_vosel_test_sub_vosel();

    printk("[exec_vsbst_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vsbst_vosel_test_sub_vosel();

    printk("[exec_vsbst_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vsbst_vosel_test_sub_vosel();
}

void exec_vsbst_vosel_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vsbst_vosel_ctrl] %d\n", i);
        mt6332_upmu_set_vsbst_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VSBST_VOSEL_MASK;k++) {
                    mt6332_upmu_set_vsbst_vosel(k); printk("[mt6332_upmu_set_vsbst_vosel] k=%d, ",k);
                    if(k==0) mdelay(500);
                    read_adc_value(VSBST_INDEX);
                }
                break;

            case 1:
                printk("[mt6332_upmu_set_vsbst_vosel_sel(0)]\n");
                mt6332_upmu_set_vsbst_vosel_sel(0);
                exec_vsbst_vosel_test_sub(); 

                printk("[mt6332_upmu_set_vsbst_vosel_sel(1)]\n");
                mt6332_upmu_set_vsbst_vosel_sel(1);                
                exec_vsbst_vosel_test_sub();

                printk("[mt6332_upmu_set_vsbst_vosel_sel(2)]\n");
                mt6332_upmu_set_vsbst_vosel_sel(2);
                exec_vsbst_vosel_test_sub();

                printk("[mt6332_upmu_set_vsbst_vosel_sel(3)]\n");
                mt6332_upmu_set_vsbst_vosel_sel(3);
                exec_vsbst_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void PMIC_BUCK_VOSEL(int index_val)
{
    printk("[PMIC_BUCK_VOSEL] start....\n");

    set_srclken_1_val(1); 
    set_srclken_2_val(1);
    set_srclken_sw_mode();

    //MT6331
    mt6331_upmu_set_vdvfs11_en_ctrl(0);
    mt6331_upmu_set_vdvfs12_en_ctrl(0);
    mt6331_upmu_set_vdvfs13_en_ctrl(0);
    mt6331_upmu_set_vdvfs14_en_ctrl(0);
    mt6331_upmu_set_vgpu_en_ctrl(0);
    mt6331_upmu_set_vcore1_en_ctrl(0);
    mt6331_upmu_set_vcore2_en_ctrl(0);
    mt6331_upmu_set_vio18_en_ctrl(0);

    //MT6332
    mt6332_upmu_set_vdram_en_ctrl(0);
    mt6332_upmu_set_vdvfs2_en_ctrl(0);
    mt6332_upmu_set_vrf1_en_ctrl(0);
    mt6332_upmu_set_vrf2_en_ctrl(0);
    mt6332_upmu_set_vpa_en_ctrl(0);
    mt6332_upmu_set_vsbst_en_ctrl(0);

    switch(index_val){
      //MT6331
      case 0 : mt6331_upmu_set_vdvfs11_en(1); exec_vdvfs11_vosel_test();     break;
      case 1 : mt6331_upmu_set_vdvfs12_en(1); exec_vdvfs12_vosel_test();     break;
      case 2 : mt6331_upmu_set_vdvfs13_en(1); exec_vdvfs13_vosel_test();     break;
      case 3 : mt6331_upmu_set_vdvfs14_en(1); exec_vdvfs14_vosel_test();     break;
      case 4 : mt6331_upmu_set_vgpu_en(1);    exec_vgpu_vosel_test();        break;
      case 5 : mt6331_upmu_set_vcore1_en(1);  exec_vcore1_vosel_test();      break;
      case 6 : mt6331_upmu_set_vcore2_en(1);  exec_vcore2_vosel_test();      break;
      case 7 : mt6331_upmu_set_vio18_en(1);   exec_vio18_vosel_test();       break;

      //MT6331
      case 8 : mt6332_upmu_set_vdram_en(1);   exec_vdram_vosel_test();       break;
      case 9 : mt6332_upmu_set_vdvfs2_en(1);  exec_vdvfs2_vosel_test();      break;
      case 10: mt6332_upmu_set_vrf1_en(1);    exec_vrf1_vosel_test();        break;
      case 11: mt6332_upmu_set_vrf2_en(1);    exec_vrf2_vosel_test();        break;
      case 12: mt6332_upmu_set_vpa_en(1);     exec_vpa_vosel_test();         break;      
      case 13: mt6332_upmu_set_vsbst_en(1);   exec_vsbst_vosel_test();       break;

      default:
        printk("[PMIC_BUCK_VOSEL] Invalid channel value(%d)\n", index_val);
        break;
    }

    printk("[PMIC_BUCK_VOSEL] end....\n");
}
#endif

#ifdef BUCK_DLC_DVT_ENABLE
void exec_vgpu_dlc_test_sub_dlc(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VGPU_DLC_SLEEP_MASK;k++) {
        mt6331_upmu_set_vgpu_dlc_sleep(k); 
        mt6331_upmu_set_vgpu_dlc_n_sleep(k);
        
        printk("[exec_vgpu_dlc_test_sub_dlc] dlc_sleep=%d, dlc_n_sleep=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6331_upmu_get_qi_vgpu_dlc(), mt6331_upmu_get_qi_vgpu_dlc_n());
    }
    
    for(k=0;k<=MT6331_PMIC_VGPU_DLC_ON_MASK;k++) {
        mt6331_upmu_set_vgpu_dlc_on(k); 
        mt6331_upmu_set_vgpu_dlc_n_on(k);
        
        printk("[exec_vgpu_dlc_test_sub_dlc] dlc_on=%d, dlc_n_on=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6331_upmu_get_qi_vgpu_dlc(), mt6331_upmu_get_qi_vgpu_dlc_n());
    }
}

void exec_vgpu_dlc_test_sub(void)
{
    printk("[exec_vgpu_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vgpu_dlc_test_sub_dlc();

    printk("[exec_vgpu_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vgpu_dlc_test_sub_dlc();

    printk("[exec_vgpu_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vgpu_dlc_test_sub_dlc();

    printk("[exec_vgpu_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vgpu_dlc_test_sub_dlc();
}

void exec_vgpu_dlc_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vgpu_dlc_ctrl] %d\n", i);
        mt6331_upmu_set_vgpu_dlc_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VGPU_DLC_MASK;k++) {
                    mt6331_upmu_set_vgpu_dlc(k); 
                    mt6331_upmu_set_vgpu_dlc_n(k);
                    
                    printk("[exec_vgpu_dlc_test] dlc=%d, dlc_n=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
                        k, k, mt6331_upmu_get_qi_vgpu_dlc(), mt6331_upmu_get_qi_vgpu_dlc_n());
                }                    
                break;

            case 1:
                printk("[mt6331_upmu_set_vgpu_dlc_sel(0)]\n");
                mt6331_upmu_set_vgpu_dlc_sel(0);
                exec_vgpu_dlc_test_sub(); 

                printk("[mt6331_upmu_set_vgpu_dlc_sel(1)]\n");
                mt6331_upmu_set_vgpu_dlc_sel(1);                
                exec_vgpu_dlc_test_sub();

                printk("[mt6331_upmu_set_vgpu_dlc_sel(2)]\n");
                mt6331_upmu_set_vgpu_dlc_sel(2);
                exec_vgpu_dlc_test_sub();

                printk("[mt6331_upmu_set_vgpu_dlc_sel(3)]\n");
                mt6331_upmu_set_vgpu_dlc_sel(3);
                exec_vgpu_dlc_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vcore1_dlc_test_sub_dlc(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VCORE1_DLC_SLEEP_MASK;k++) {
        mt6331_upmu_set_vcore1_dlc_sleep(k); 
        mt6331_upmu_set_vcore1_dlc_n_sleep(k);
        
        printk("[exec_vcore1_dlc_test_sub_dlc] dlc_sleep=%d, dlc_n_sleep=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6331_upmu_get_qi_vcore1_dlc(), mt6331_upmu_get_qi_vcore1_dlc_n());
    }
    
    for(k=0;k<=MT6331_PMIC_VCORE1_DLC_ON_MASK;k++) {
        mt6331_upmu_set_vcore1_dlc_on(k); 
        mt6331_upmu_set_vcore1_dlc_n_on(k);
        
        printk("[exec_vcore1_dlc_test_sub_dlc] dlc_on=%d, dlc_n_on=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6331_upmu_get_qi_vcore1_dlc(), mt6331_upmu_get_qi_vcore1_dlc_n());
    }
}

void exec_vcore1_dlc_test_sub(void)
{
    printk("[exec_vcore1_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vcore1_dlc_test_sub_dlc();

    printk("[exec_vcore1_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vcore1_dlc_test_sub_dlc();

    printk("[exec_vcore1_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vcore1_dlc_test_sub_dlc();

    printk("[exec_vcore1_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vcore1_dlc_test_sub_dlc();
}

void exec_vcore1_dlc_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vcore1_dlc_ctrl] %d\n", i);
        mt6331_upmu_set_vcore1_dlc_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VCORE1_DLC_MASK;k++) {
                    mt6331_upmu_set_vcore1_dlc(k); 
                    mt6331_upmu_set_vcore1_dlc_n(k);
                    
                    printk("[exec_vcore1_dlc_test] dlc=%d, dlc_n=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
                        k, k, mt6331_upmu_get_qi_vcore1_dlc(), mt6331_upmu_get_qi_vcore1_dlc_n());
                }                    
                break;

            case 1:
                printk("[mt6331_upmu_set_vcore1_dlc_sel(0)]\n");
                mt6331_upmu_set_vcore1_dlc_sel(0);
                exec_vcore1_dlc_test_sub(); 

                printk("[mt6331_upmu_set_vcore1_dlc_sel(1)]\n");
                mt6331_upmu_set_vcore1_dlc_sel(1);                
                exec_vcore1_dlc_test_sub();

                printk("[mt6331_upmu_set_vcore1_dlc_sel(2)]\n");
                mt6331_upmu_set_vcore1_dlc_sel(2);
                exec_vcore1_dlc_test_sub();

                printk("[mt6331_upmu_set_vcore1_dlc_sel(3)]\n");
                mt6331_upmu_set_vcore1_dlc_sel(3);
                exec_vcore1_dlc_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vcore2_dlc_test_sub_dlc(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VCORE2_DLC_SLEEP_MASK;k++) {
        mt6331_upmu_set_vcore2_dlc_sleep(k); 
        mt6331_upmu_set_vcore2_dlc_n_sleep(k);
        
        printk("[exec_vcore2_dlc_test_sub_dlc] dlc_sleep=%d, dlc_n_sleep=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6331_upmu_get_qi_vcore2_dlc(), mt6331_upmu_get_qi_vcore2_dlc_n());
    }
    
    for(k=0;k<=MT6331_PMIC_VCORE2_DLC_ON_MASK;k++) {
        mt6331_upmu_set_vcore2_dlc_on(k); 
        mt6331_upmu_set_vcore2_dlc_n_on(k);
        
        printk("[exec_vcore2_dlc_test_sub_dlc] dlc_on=%d, dlc_n_on=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6331_upmu_get_qi_vcore2_dlc(), mt6331_upmu_get_qi_vcore2_dlc_n());
    }
}

void exec_vcore2_dlc_test_sub(void)
{
    printk("[exec_vcore2_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vcore2_dlc_test_sub_dlc();

    printk("[exec_vcore2_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vcore2_dlc_test_sub_dlc();

    printk("[exec_vcore2_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vcore2_dlc_test_sub_dlc();

    printk("[exec_vcore2_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vcore2_dlc_test_sub_dlc();
}

void exec_vcore2_dlc_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vcore2_dlc_ctrl] %d\n", i);
        mt6331_upmu_set_vcore2_dlc_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VCORE2_DLC_MASK;k++) {
                    mt6331_upmu_set_vcore2_dlc(k); 
                    mt6331_upmu_set_vcore2_dlc_n(k);
                    
                    printk("[exec_vcore2_dlc_test] dlc=%d, dlc_n=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
                        k, k, mt6331_upmu_get_qi_vcore2_dlc(), mt6331_upmu_get_qi_vcore2_dlc_n());
                }                    
                break;

            case 1:
                printk("[mt6331_upmu_set_vcore2_dlc_sel(0)]\n");
                mt6331_upmu_set_vcore2_dlc_sel(0);
                exec_vcore2_dlc_test_sub(); 

                printk("[mt6331_upmu_set_vcore2_dlc_sel(1)]\n");
                mt6331_upmu_set_vcore2_dlc_sel(1);                
                exec_vcore2_dlc_test_sub();

                printk("[mt6331_upmu_set_vcore2_dlc_sel(2)]\n");
                mt6331_upmu_set_vcore2_dlc_sel(2);
                exec_vcore2_dlc_test_sub();

                printk("[mt6331_upmu_set_vcore2_dlc_sel(3)]\n");
                mt6331_upmu_set_vcore2_dlc_sel(3);
                exec_vcore2_dlc_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vio18_dlc_test_sub_dlc(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VIO18_DLC_SLEEP_MASK;k++) {
        mt6331_upmu_set_vio18_dlc_sleep(k); 
        mt6331_upmu_set_vio18_dlc_n_sleep(k);
        
        printk("[exec_vio18_dlc_test_sub_dlc] dlc_sleep=%d, dlc_n_sleep=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6331_upmu_get_qi_vio18_dlc(), mt6331_upmu_get_qi_vio18_dlc_n());
    }
    
    for(k=0;k<=MT6331_PMIC_VIO18_DLC_ON_MASK;k++) {
        mt6331_upmu_set_vio18_dlc_on(k); 
        mt6331_upmu_set_vio18_dlc_n_on(k);
        
        printk("[exec_vio18_dlc_test_sub_dlc] dlc_on=%d, dlc_n_on=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6331_upmu_get_qi_vio18_dlc(), mt6331_upmu_get_qi_vio18_dlc_n());
    }
}

void exec_vio18_dlc_test_sub(void)
{
    printk("[exec_vio18_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vio18_dlc_test_sub_dlc();

    printk("[exec_vio18_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vio18_dlc_test_sub_dlc();

    printk("[exec_vio18_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vio18_dlc_test_sub_dlc();

    printk("[exec_vio18_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vio18_dlc_test_sub_dlc();
}

void exec_vio18_dlc_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vio18_dlc_ctrl] %d\n", i);
        mt6331_upmu_set_vio18_dlc_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VIO18_DLC_MASK;k++) {
                    mt6331_upmu_set_vio18_dlc(k); 
                    mt6331_upmu_set_vio18_dlc_n(k);
                    
                    printk("[exec_vio18_dlc_test] dlc=%d, dlc_n=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
                        k, k, mt6331_upmu_get_qi_vio18_dlc(), mt6331_upmu_get_qi_vio18_dlc_n());
                }                    
                break;

            case 1:
                printk("[mt6331_upmu_set_vio18_dlc_sel(0)]\n");
                mt6331_upmu_set_vio18_dlc_sel(0);
                exec_vio18_dlc_test_sub(); 

                printk("[mt6331_upmu_set_vio18_dlc_sel(1)]\n");
                mt6331_upmu_set_vio18_dlc_sel(1);                
                exec_vio18_dlc_test_sub();

                printk("[mt6331_upmu_set_vio18_dlc_sel(2)]\n");
                mt6331_upmu_set_vio18_dlc_sel(2);
                exec_vio18_dlc_test_sub();

                printk("[mt6331_upmu_set_vio18_dlc_sel(3)]\n");
                mt6331_upmu_set_vio18_dlc_sel(3);
                exec_vio18_dlc_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vdram_dlc_test_sub_dlc(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VDRAM_DLC_SLEEP_MASK;k++) {
        mt6332_upmu_set_vdram_dlc_sleep(k); 
        mt6332_upmu_set_vdram_dlc_n_sleep(k);
        
        printk("[exec_vdram_dlc_test_sub_dlc] dlc_sleep=%d, dlc_n_sleep=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6332_upmu_get_qi_vdram_dlc(), mt6332_upmu_get_qi_vdram_dlc_n());
    }
    
    for(k=0;k<=MT6332_PMIC_VDRAM_DLC_ON_MASK;k++) {
        mt6332_upmu_set_vdram_dlc_on(k); 
        mt6332_upmu_set_vdram_dlc_n_on(k);
        
        printk("[exec_vdram_dlc_test_sub_dlc] dlc_on=%d, dlc_n_on=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6332_upmu_get_qi_vdram_dlc(), mt6332_upmu_get_qi_vdram_dlc_n());
    }
}

void exec_vdram_dlc_test_sub(void)
{
    printk("[exec_vdram_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vdram_dlc_test_sub_dlc();

    printk("[exec_vdram_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vdram_dlc_test_sub_dlc();

    printk("[exec_vdram_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vdram_dlc_test_sub_dlc();

    printk("[exec_vdram_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vdram_dlc_test_sub_dlc();
}

void exec_vdram_dlc_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vdram_dlc_ctrl] %d\n", i);
        mt6332_upmu_set_vdram_dlc_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VDRAM_DLC_MASK;k++) {
                    mt6332_upmu_set_vdram_dlc(k); 
                    mt6332_upmu_set_vdram_dlc_n(k);
                    
                    printk("[exec_vdram_dlc_test] dlc=%d, dlc_n=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
                        k, k, mt6332_upmu_get_qi_vdram_dlc(), mt6332_upmu_get_qi_vdram_dlc_n());
                }                    
                break;

            case 1:
                printk("[mt6332_upmu_set_vdram_dlc_sel(0)]\n");
                mt6332_upmu_set_vdram_dlc_sel(0);
                exec_vdram_dlc_test_sub(); 

                printk("[mt6332_upmu_set_vdram_dlc_sel(1)]\n");
                mt6332_upmu_set_vdram_dlc_sel(1);                
                exec_vdram_dlc_test_sub();

                printk("[mt6332_upmu_set_vdram_dlc_sel(2)]\n");
                mt6332_upmu_set_vdram_dlc_sel(2);
                exec_vdram_dlc_test_sub();

                printk("[mt6332_upmu_set_vdram_dlc_sel(3)]\n");
                mt6332_upmu_set_vdram_dlc_sel(3);
                exec_vdram_dlc_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vdvfs2_dlc_test_sub_dlc(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VDVFS2_DLC_SLEEP_MASK;k++) {
        mt6332_upmu_set_vdvfs2_dlc_sleep(k); 
        mt6332_upmu_set_vdvfs2_dlc_n_sleep(k);
        
        printk("[exec_vdvfs2_dlc_test_sub_dlc] dlc_sleep=%d, dlc_n_sleep=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6332_upmu_get_qi_vdvfs2_dlc(), mt6332_upmu_get_qi_vdvfs2_dlc_n());
    }
    
    for(k=0;k<=MT6332_PMIC_VDVFS2_DLC_ON_MASK;k++) {
        mt6332_upmu_set_vdvfs2_dlc_on(k); 
        mt6332_upmu_set_vdvfs2_dlc_n_on(k);
        
        printk("[exec_vdvfs2_dlc_test_sub_dlc] dlc_on=%d, dlc_n_on=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6332_upmu_get_qi_vdvfs2_dlc(), mt6332_upmu_get_qi_vdvfs2_dlc_n());
    }
}

void exec_vdvfs2_dlc_test_sub(void)
{
    printk("[exec_vdvfs2_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vdvfs2_dlc_test_sub_dlc();

    printk("[exec_vdvfs2_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vdvfs2_dlc_test_sub_dlc();

    printk("[exec_vdvfs2_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vdvfs2_dlc_test_sub_dlc();

    printk("[exec_vdvfs2_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vdvfs2_dlc_test_sub_dlc();
}

void exec_vdvfs2_dlc_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vdvfs2_dlc_ctrl] %d\n", i);
        mt6332_upmu_set_vdvfs2_dlc_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VDVFS2_DLC_MASK;k++) {
                    mt6332_upmu_set_vdvfs2_dlc(k); 
                    mt6332_upmu_set_vdvfs2_dlc_n(k);
                    
                    printk("[exec_vdvfs2_dlc_test] dlc=%d, dlc_n=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
                        k, k, mt6332_upmu_get_qi_vdvfs2_dlc(), mt6332_upmu_get_qi_vdvfs2_dlc_n());
                }                    
                break;

            case 1:
                printk("[mt6332_upmu_set_vdvfs2_dlc_sel(0)]\n");
                mt6332_upmu_set_vdvfs2_dlc_sel(0);
                exec_vdvfs2_dlc_test_sub(); 

                printk("[mt6332_upmu_set_vdvfs2_dlc_sel(1)]\n");
                mt6332_upmu_set_vdvfs2_dlc_sel(1);                
                exec_vdvfs2_dlc_test_sub();

                printk("[mt6332_upmu_set_vdvfs2_dlc_sel(2)]\n");
                mt6332_upmu_set_vdvfs2_dlc_sel(2);
                exec_vdvfs2_dlc_test_sub();

                printk("[mt6332_upmu_set_vdvfs2_dlc_sel(3)]\n");
                mt6332_upmu_set_vdvfs2_dlc_sel(3);
                exec_vdvfs2_dlc_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vrf1_dlc_test_sub_dlc(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VRF1_DLC_SLEEP_MASK;k++) {
        mt6332_upmu_set_vrf1_dlc_sleep(k); 
        mt6332_upmu_set_vrf1_dlc_n_sleep(k);
        
        printk("[exec_vrf1_dlc_test_sub_dlc] dlc_sleep=%d, dlc_n_sleep=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6332_upmu_get_qi_vrf1_dlc(), mt6332_upmu_get_qi_vrf1_dlc_n());
    }
    
    for(k=0;k<=MT6332_PMIC_VRF1_DLC_ON_MASK;k++) {
        mt6332_upmu_set_vrf1_dlc_on(k); 
        mt6332_upmu_set_vrf1_dlc_n_on(k);
        
        printk("[exec_vrf1_dlc_test_sub_dlc] dlc_on=%d, dlc_n_on=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6332_upmu_get_qi_vrf1_dlc(), mt6332_upmu_get_qi_vrf1_dlc_n());
    }
}

void exec_vrf1_dlc_test_sub(void)
{
    printk("[exec_vrf1_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vrf1_dlc_test_sub_dlc();

    printk("[exec_vrf1_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vrf1_dlc_test_sub_dlc();

    printk("[exec_vrf1_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vrf1_dlc_test_sub_dlc();

    printk("[exec_vrf1_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vrf1_dlc_test_sub_dlc();
}

void exec_vrf1_dlc_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vrf1_dlc_ctrl] %d\n", i);
        mt6332_upmu_set_vrf1_dlc_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VRF1_DLC_MASK;k++) {
                    mt6332_upmu_set_vrf1_dlc(k); 
                    mt6332_upmu_set_vrf1_dlc_n(k);
                    
                    printk("[exec_vrf1_dlc_test] dlc=%d, dlc_n=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
                        k, k, mt6332_upmu_get_qi_vrf1_dlc(), mt6332_upmu_get_qi_vrf1_dlc_n());
                }                    
                break;

            case 1:
                printk("[mt6332_upmu_set_vrf1_dlc_sel(0)]\n");
                mt6332_upmu_set_vrf1_dlc_sel(0);
                exec_vrf1_dlc_test_sub(); 

                printk("[mt6332_upmu_set_vrf1_dlc_sel(1)]\n");
                mt6332_upmu_set_vrf1_dlc_sel(1);                
                exec_vrf1_dlc_test_sub();

                printk("[mt6332_upmu_set_vrf1_dlc_sel(2)]\n");
                mt6332_upmu_set_vrf1_dlc_sel(2);
                exec_vrf1_dlc_test_sub();

                printk("[mt6332_upmu_set_vrf1_dlc_sel(3)]\n");
                mt6332_upmu_set_vrf1_dlc_sel(3);
                exec_vrf1_dlc_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vrf2_dlc_test_sub_dlc(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VRF2_DLC_SLEEP_MASK;k++) {
        mt6332_upmu_set_vrf2_dlc_sleep(k); 
        mt6332_upmu_set_vrf2_dlc_n_sleep(k);
        
        printk("[exec_vrf2_dlc_test_sub_dlc] dlc_sleep=%d, dlc_n_sleep=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6332_upmu_get_qi_vrf2_dlc(), mt6332_upmu_get_qi_vrf2_dlc_n());
    }
    
    for(k=0;k<=MT6332_PMIC_VRF2_DLC_ON_MASK;k++) {
        mt6332_upmu_set_vrf2_dlc_on(k); 
        mt6332_upmu_set_vrf2_dlc_n_on(k);
        
        printk("[exec_vrf2_dlc_test_sub_dlc] dlc_on=%d, dlc_n_on=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
            k, k, mt6332_upmu_get_qi_vrf2_dlc(), mt6332_upmu_get_qi_vrf2_dlc_n());
    }
}

void exec_vrf2_dlc_test_sub(void)
{
    printk("[exec_vrf2_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vrf2_dlc_test_sub_dlc();

    printk("[exec_vrf2_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vrf2_dlc_test_sub_dlc();

    printk("[exec_vrf2_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vrf2_dlc_test_sub_dlc();

    printk("[exec_vrf2_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vrf2_dlc_test_sub_dlc();
}

void exec_vrf2_dlc_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vrf2_dlc_ctrl] %d\n", i);
        mt6332_upmu_set_vrf2_dlc_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VRF2_DLC_MASK;k++) {
                    mt6332_upmu_set_vrf2_dlc(k); 
                    mt6332_upmu_set_vrf2_dlc_n(k);
                    
                    printk("[exec_vrf2_dlc_test] dlc=%d, dlc_n=%d, qi_dlc=%d, qi_dlc_n=%d\n", 
                        k, k, mt6332_upmu_get_qi_vrf2_dlc(), mt6332_upmu_get_qi_vrf2_dlc_n());
                }                    
                break;

            case 1:
                printk("[mt6332_upmu_set_vrf2_dlc_sel(0)]\n");
                mt6332_upmu_set_vrf2_dlc_sel(0);
                exec_vrf2_dlc_test_sub(); 

                printk("[mt6332_upmu_set_vrf2_dlc_sel(1)]\n");
                mt6332_upmu_set_vrf2_dlc_sel(1);                
                exec_vrf2_dlc_test_sub();

                printk("[mt6332_upmu_set_vrf2_dlc_sel(2)]\n");
                mt6332_upmu_set_vrf2_dlc_sel(2);
                exec_vrf2_dlc_test_sub();

                printk("[mt6332_upmu_set_vrf2_dlc_sel(3)]\n");
                mt6332_upmu_set_vrf2_dlc_sel(3);
                exec_vrf2_dlc_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vpa_dlc_test_sub_dlc(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VPA_DLC_SLEEP_MASK;k++) {
        mt6332_upmu_set_vpa_dlc_sleep(k);         
        printk("[exec_vpa_dlc_test_sub_dlc] dlc_sleep=%d, qi_dlc=%d\n", k, mt6332_upmu_get_qi_vpa_dlc());
    }
    
    for(k=0;k<=MT6332_PMIC_VPA_DLC_ON_MASK;k++) {
        mt6332_upmu_set_vpa_dlc_on(k);         
        printk("[exec_vpa_dlc_test_sub_dlc] dlc_on=%d, qi_dlc=%d\n", k, mt6332_upmu_get_qi_vpa_dlc());
    }
}

void exec_vpa_dlc_test_sub(void)
{
    printk("[exec_vpa_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vpa_dlc_test_sub_dlc();

    printk("[exec_vpa_dlc_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vpa_dlc_test_sub_dlc();

    printk("[exec_vpa_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vpa_dlc_test_sub_dlc();

    printk("[exec_vpa_dlc_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vpa_dlc_test_sub_dlc();
}

void exec_vpa_dlc_test(void)
{
    int i=0, k=0;

//-----------------------------------------------------------------
    printk("[exec_vpa_dlc_test] mt6332_upmu_set_vpa_dlc_map_en(0);\n");
    mt6332_upmu_set_vpa_dlc_map_en(0);
//-----------------------------------------------------------------
    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vpa_dlc_ctrl] %d\n", i);
        mt6332_upmu_set_vpa_dlc_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VPA_DLC_MASK;k++) {
                    mt6332_upmu_set_vpa_dlc(k);                     
                    printk("[exec_vpa_dlc_test] dlc=%d, qi_dlc=%d\n", k, mt6332_upmu_get_qi_vpa_dlc());
                }                    
                break;

            case 1:
                printk("[mt6332_upmu_set_vpa_dlc_sel(0)]\n");
                mt6332_upmu_set_vpa_dlc_sel(0);
                exec_vpa_dlc_test_sub(); 

                printk("[mt6332_upmu_set_vpa_dlc_sel(1)]\n");
                mt6332_upmu_set_vpa_dlc_sel(1);                
                exec_vpa_dlc_test_sub();

                printk("[mt6332_upmu_set_vpa_dlc_sel(2)]\n");
                mt6332_upmu_set_vpa_dlc_sel(2);
                exec_vpa_dlc_test_sub();

                printk("[mt6332_upmu_set_vpa_dlc_sel(3)]\n");
                mt6332_upmu_set_vpa_dlc_sel(3);
                exec_vpa_dlc_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
//-----------------------------------------------------------------
    printk("[exec_vpa_dlc_test] mt6332_upmu_set_vpa_dlc_map_en(1);\n");
    mt6332_upmu_set_vpa_dlc_map_en(1);
//-----------------------------------------------------------------
    printk("[exec_vpa_dlc_test] mt6332_upmu_set_vpa_vosel_dlc001(20);\n");
    printk("[exec_vpa_dlc_test] mt6332_upmu_set_vpa_vosel_dlc011(30);\n");
    printk("[exec_vpa_dlc_test] mt6332_upmu_set_vpa_vosel_dlc111(50);\n");
    mt6332_upmu_set_vpa_vosel_dlc001(20);
    mt6332_upmu_set_vpa_vosel_dlc011(30);
    mt6332_upmu_set_vpa_vosel_dlc111(50);

    printk("[exec_vpa_dlc_test] mt6332_upmu_set_vpa_vosel_ctrl(0);\n");
    mt6332_upmu_set_vpa_vosel_ctrl(0);
    
    for(k=0;k<=MT6332_PMIC_VPA_VOSEL_MASK;k++) {
        mt6332_upmu_set_vpa_vosel(k); 
        printk("[exec_vpa_dlc_test] ni_vosel=%d, qi_dlc=%d\n", mt6332_upmu_get_ni_vpa_vosel(), mt6332_upmu_get_qi_vpa_dlc());
    }
//-----------------------------------------------------------------
    printk("[exec_vpa_dlc_test] mt6332_upmu_set_vpa_dlc_map_en(0);\n");
    mt6332_upmu_set_vpa_dlc_map_en(0);
//-----------------------------------------------------------------

}

void PMIC_BUCK_DLC(int index_val)
{
    printk("[PMIC_BUCK_DLC] start....\n");

    set_srclken_1_val(1); 
    set_srclken_2_val(1);
    set_srclken_sw_mode();

    //MT6331
    mt6331_upmu_set_vdvfs11_en_ctrl(0);
    mt6331_upmu_set_vdvfs12_en_ctrl(0);
    mt6331_upmu_set_vdvfs13_en_ctrl(0);
    mt6331_upmu_set_vdvfs14_en_ctrl(0);
    mt6331_upmu_set_vgpu_en_ctrl(0);
    mt6331_upmu_set_vcore1_en_ctrl(0);
    mt6331_upmu_set_vcore2_en_ctrl(0);
    mt6331_upmu_set_vio18_en_ctrl(0);

    //MT6332
    mt6332_upmu_set_vdram_en_ctrl(0);
    mt6332_upmu_set_vdvfs2_en_ctrl(0);
    mt6332_upmu_set_vrf1_en_ctrl(0);
    mt6332_upmu_set_vrf2_en_ctrl(0);
    mt6332_upmu_set_vpa_en_ctrl(0);
    mt6332_upmu_set_vsbst_en_ctrl(0);

    switch(index_val){
      //MT6331
      case 0 : printk("[PMIC_BUCK_DLC] no DLC function\n");                break;
      case 1 : printk("[PMIC_BUCK_DLC] no DLC function\n");                break;
      case 2 : printk("[PMIC_BUCK_DLC] no DLC function\n");                break;
      case 3 : printk("[PMIC_BUCK_DLC] no DLC function\n");                break;
      case 4 : mt6331_upmu_set_vgpu_en(1);    exec_vgpu_dlc_test();        break;
      case 5 : mt6331_upmu_set_vcore1_en(1);  exec_vcore1_dlc_test();      break;
      case 6 : mt6331_upmu_set_vcore2_en(1);  exec_vcore2_dlc_test();      break;
      case 7 : mt6331_upmu_set_vio18_en(1);   exec_vio18_dlc_test();       break;

      //MT6331
      case 8 : mt6332_upmu_set_vdram_en(1);   exec_vdram_dlc_test();       break;
      case 9 : mt6332_upmu_set_vdvfs2_en(1);  exec_vdvfs2_dlc_test();      break;
      case 10: mt6332_upmu_set_vrf1_en(1);    exec_vrf1_dlc_test();        break;
      case 11: mt6332_upmu_set_vrf2_en(1);    exec_vrf2_dlc_test();        break;
      case 12: mt6332_upmu_set_vpa_en(1);     exec_vpa_dlc_test();         break;      
      case 13: printk("[PMIC_BUCK_DLC] no DLC function\n");                break;

      default:
        printk("[PMIC_BUCK_DLC] Invalid channel value(%d)\n", index_val);
        break;
    }

    printk("[PMIC_BUCK_DLC] end....\n");
}
#endif

#ifdef BUCK_BURST_DVT_ENABLE
void exec_vgpu_burst_test_sub_burst(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VGPU_BURST_SLEEP_MASK;k++) {
        mt6331_upmu_set_vgpu_burst_sleep(k);         
        printk("[exec_vgpu_burst_test_sub_burst] burst_sleep=%d, qi_burst=%d\n", k, mt6331_upmu_get_qi_vgpu_burst());
    }
    
    for(k=0;k<=MT6331_PMIC_VGPU_BURST_ON_MASK;k++) {
        mt6331_upmu_set_vgpu_burst_on(k);        
        printk("[exec_vgpu_burst_test_sub_burst] burst_on=%d, qi_burst=%d\n", k, mt6331_upmu_get_qi_vgpu_burst());
    }
}

void exec_vgpu_burst_test_sub(void)
{
    printk("[exec_vgpu_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vgpu_burst_test_sub_burst();

    printk("[exec_vgpu_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vgpu_burst_test_sub_burst();

    printk("[exec_vgpu_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vgpu_burst_test_sub_burst();

    printk("[exec_vgpu_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vgpu_burst_test_sub_burst();
}

void exec_vgpu_burst_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vgpu_burst_ctrl] %d\n", i);
        mt6331_upmu_set_vgpu_burst_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VGPU_BURST_MASK;k++) {
                    mt6331_upmu_set_vgpu_burst(k);                     
                    printk("[exec_vgpu_burst_test] burst=%d, qi_burst=%d\n", k, mt6331_upmu_get_qi_vgpu_burst());
                }                    
                break;

            case 1:
                printk("[mt6331_upmu_set_vgpu_burst_sel(0)]\n");
                mt6331_upmu_set_vgpu_burst_sel(0);
                exec_vgpu_burst_test_sub(); 

                printk("[mt6331_upmu_set_vgpu_burst_sel(1)]\n");
                mt6331_upmu_set_vgpu_burst_sel(1);                
                exec_vgpu_burst_test_sub();

                printk("[mt6331_upmu_set_vgpu_burst_sel(2)]\n");
                mt6331_upmu_set_vgpu_burst_sel(2);
                exec_vgpu_burst_test_sub();

                printk("[mt6331_upmu_set_vgpu_burst_sel(3)]\n");
                mt6331_upmu_set_vgpu_burst_sel(3);
                exec_vgpu_burst_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vcore1_burst_test_sub_burst(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VCORE1_BURST_SLEEP_MASK;k++) {
        mt6331_upmu_set_vcore1_burst_sleep(k);         
        printk("[exec_vcore1_burst_test_sub_burst] burst_sleep=%d, qi_burst=%d\n", k, mt6331_upmu_get_qi_vcore1_burst());
    }
    
    for(k=0;k<=MT6331_PMIC_VCORE1_BURST_ON_MASK;k++) {
        mt6331_upmu_set_vcore1_burst_on(k);        
        printk("[exec_vcore1_burst_test_sub_burst] burst_on=%d, qi_burst=%d\n", k, mt6331_upmu_get_qi_vcore1_burst());
    }
}

void exec_vcore1_burst_test_sub(void)
{
    printk("[exec_vcore1_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vcore1_burst_test_sub_burst();

    printk("[exec_vcore1_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vcore1_burst_test_sub_burst();

    printk("[exec_vcore1_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vcore1_burst_test_sub_burst();

    printk("[exec_vcore1_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vcore1_burst_test_sub_burst();
}

void exec_vcore1_burst_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vcore1_burst_ctrl] %d\n", i);
        mt6331_upmu_set_vcore1_burst_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VCORE1_BURST_MASK;k++) {
                    mt6331_upmu_set_vcore1_burst(k);                     
                    printk("[exec_vcore1_burst_test] burst=%d, qi_burst=%d\n", k, mt6331_upmu_get_qi_vcore1_burst());
                }                    
                break;

            case 1:
                printk("[mt6331_upmu_set_vcore1_burst_sel(0)]\n");
                mt6331_upmu_set_vcore1_burst_sel(0);
                exec_vcore1_burst_test_sub(); 

                printk("[mt6331_upmu_set_vcore1_burst_sel(1)]\n");
                mt6331_upmu_set_vcore1_burst_sel(1);                
                exec_vcore1_burst_test_sub();

                printk("[mt6331_upmu_set_vcore1_burst_sel(2)]\n");
                mt6331_upmu_set_vcore1_burst_sel(2);
                exec_vcore1_burst_test_sub();

                printk("[mt6331_upmu_set_vcore1_burst_sel(3)]\n");
                mt6331_upmu_set_vcore1_burst_sel(3);
                exec_vcore1_burst_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vcore2_burst_test_sub_burst(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VCORE2_BURST_SLEEP_MASK;k++) {
        mt6331_upmu_set_vcore2_burst_sleep(k);         
        printk("[exec_vcore2_burst_test_sub_burst] burst_sleep=%d, qi_burst=%d\n", k, mt6331_upmu_get_qi_vcore2_burst());
    }
    
    for(k=0;k<=MT6331_PMIC_VCORE2_BURST_ON_MASK;k++) {
        mt6331_upmu_set_vcore2_burst_on(k);        
        printk("[exec_vcore2_burst_test_sub_burst] burst_on=%d, qi_burst=%d\n", k, mt6331_upmu_get_qi_vcore2_burst());
    }
}

void exec_vcore2_burst_test_sub(void)
{
    printk("[exec_vcore2_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vcore2_burst_test_sub_burst();

    printk("[exec_vcore2_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vcore2_burst_test_sub_burst();

    printk("[exec_vcore2_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vcore2_burst_test_sub_burst();

    printk("[exec_vcore2_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vcore2_burst_test_sub_burst();
}

void exec_vcore2_burst_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vcore2_burst_ctrl] %d\n", i);
        mt6331_upmu_set_vcore2_burst_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VCORE2_BURST_MASK;k++) {
                    mt6331_upmu_set_vcore2_burst(k);                     
                    printk("[exec_vcore2_burst_test] burst=%d, qi_burst=%d\n", k, mt6331_upmu_get_qi_vcore2_burst());
                }                    
                break;

            case 1:
                printk("[mt6331_upmu_set_vcore2_burst_sel(0)]\n");
                mt6331_upmu_set_vcore2_burst_sel(0);
                exec_vcore2_burst_test_sub(); 

                printk("[mt6331_upmu_set_vcore2_burst_sel(1)]\n");
                mt6331_upmu_set_vcore2_burst_sel(1);                
                exec_vcore2_burst_test_sub();

                printk("[mt6331_upmu_set_vcore2_burst_sel(2)]\n");
                mt6331_upmu_set_vcore2_burst_sel(2);
                exec_vcore2_burst_test_sub();

                printk("[mt6331_upmu_set_vcore2_burst_sel(3)]\n");
                mt6331_upmu_set_vcore2_burst_sel(3);
                exec_vcore2_burst_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vio18_burst_test_sub_burst(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_VCORE2_BURST_SLEEP_MASK;k++) {
        mt6331_upmu_set_vio18_burst_sleep(k);         
        printk("[exec_vio18_burst_test_sub_burst] burst_sleep=%d, qi_burst=%d\n", k, mt6331_upmu_get_qi_vio18_burst());
    }
    
    for(k=0;k<=MT6331_PMIC_VCORE2_BURST_ON_MASK;k++) {
        mt6331_upmu_set_vio18_burst_on(k);        
        printk("[exec_vio18_burst_test_sub_burst] burst_on=%d, qi_burst=%d\n", k, mt6331_upmu_get_qi_vio18_burst());
    }
}

void exec_vio18_burst_test_sub(void)
{
    printk("[exec_vio18_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vio18_burst_test_sub_burst();

    printk("[exec_vio18_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vio18_burst_test_sub_burst();

    printk("[exec_vio18_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vio18_burst_test_sub_burst();

    printk("[exec_vio18_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vio18_burst_test_sub_burst();
}

void exec_vio18_burst_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_vio18_burst_ctrl] %d\n", i);
        mt6331_upmu_set_vio18_burst_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_VCORE2_BURST_MASK;k++) {
                    mt6331_upmu_set_vio18_burst(k);                     
                    printk("[exec_vio18_burst_test] burst=%d, qi_burst=%d\n", k, mt6331_upmu_get_qi_vio18_burst());
                }                    
                break;

            case 1:
                printk("[mt6331_upmu_set_vio18_burst_sel(0)]\n");
                mt6331_upmu_set_vio18_burst_sel(0);
                exec_vio18_burst_test_sub(); 

                printk("[mt6331_upmu_set_vio18_burst_sel(1)]\n");
                mt6331_upmu_set_vio18_burst_sel(1);                
                exec_vio18_burst_test_sub();

                printk("[mt6331_upmu_set_vio18_burst_sel(2)]\n");
                mt6331_upmu_set_vio18_burst_sel(2);
                exec_vio18_burst_test_sub();

                printk("[mt6331_upmu_set_vio18_burst_sel(3)]\n");
                mt6331_upmu_set_vio18_burst_sel(3);
                exec_vio18_burst_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vdram_burst_test_sub_burst(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VDRAM_BURST_SLEEP_MASK;k++) {
        mt6332_upmu_set_vdram_burst_sleep(k);         
        printk("[exec_vdram_burst_test_sub_burst] burst_sleep=%d, qi_burst=%d\n", k, mt6332_upmu_get_qi_vdram_burst());
    }
    
    for(k=0;k<=MT6332_PMIC_VDRAM_BURST_ON_MASK;k++) {
        mt6332_upmu_set_vdram_burst_on(k);        
        printk("[exec_vdram_burst_test_sub_burst] burst_on=%d, qi_burst=%d\n", k, mt6332_upmu_get_qi_vdram_burst());
    }
}

void exec_vdram_burst_test_sub(void)
{
    printk("[exec_vdram_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vdram_burst_test_sub_burst();

    printk("[exec_vdram_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vdram_burst_test_sub_burst();

    printk("[exec_vdram_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vdram_burst_test_sub_burst();

    printk("[exec_vdram_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vdram_burst_test_sub_burst();
}

void exec_vdram_burst_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vdram_burst_ctrl] %d\n", i);
        mt6332_upmu_set_vdram_burst_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VDRAM_BURST_MASK;k++) {
                    mt6332_upmu_set_vdram_burst(k);                     
                    printk("[exec_vdram_burst_test] burst=%d, qi_burst=%d\n", k, mt6332_upmu_get_qi_vdram_burst());
                }                    
                break;

            case 1:
                printk("[mt6332_upmu_set_vdram_burst_sel(0)]\n");
                mt6332_upmu_set_vdram_burst_sel(0);
                exec_vdram_burst_test_sub(); 

                printk("[mt6332_upmu_set_vdram_burst_sel(1)]\n");
                mt6332_upmu_set_vdram_burst_sel(1);                
                exec_vdram_burst_test_sub();

                printk("[mt6332_upmu_set_vdram_burst_sel(2)]\n");
                mt6332_upmu_set_vdram_burst_sel(2);
                exec_vdram_burst_test_sub();

                printk("[mt6332_upmu_set_vdram_burst_sel(3)]\n");
                mt6332_upmu_set_vdram_burst_sel(3);
                exec_vdram_burst_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vdvfs2_burst_test_sub_burst(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VDVFS2_BURST_SLEEP_MASK;k++) {
        mt6332_upmu_set_vdvfs2_burst_sleep(k);         
        printk("[exec_vdvfs2_burst_test_sub_burst] burst_sleep=%d, qi_burst=%d\n", k, mt6332_upmu_get_qi_vdvfs2_burst());
    }
    
    for(k=0;k<=MT6332_PMIC_VDVFS2_BURST_ON_MASK;k++) {
        mt6332_upmu_set_vdvfs2_burst_on(k);        
        printk("[exec_vdvfs2_burst_test_sub_burst] burst_on=%d, qi_burst=%d\n", k, mt6332_upmu_get_qi_vdvfs2_burst());
    }
}

void exec_vdvfs2_burst_test_sub(void)
{
    printk("[exec_vdvfs2_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vdvfs2_burst_test_sub_burst();

    printk("[exec_vdvfs2_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vdvfs2_burst_test_sub_burst();

    printk("[exec_vdvfs2_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vdvfs2_burst_test_sub_burst();

    printk("[exec_vdvfs2_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vdvfs2_burst_test_sub_burst();
}

void exec_vdvfs2_burst_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vdvfs2_burst_ctrl] %d\n", i);
        mt6332_upmu_set_vdvfs2_burst_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VDVFS2_BURST_MASK;k++) {
                    mt6332_upmu_set_vdvfs2_burst(k);                     
                    printk("[exec_vdvfs2_burst_test] burst=%d, qi_burst=%d\n", k, mt6332_upmu_get_qi_vdvfs2_burst());
                }                    
                break;

            case 1:
                printk("[mt6332_upmu_set_vdvfs2_burst_sel(0)]\n");
                mt6332_upmu_set_vdvfs2_burst_sel(0);
                exec_vdvfs2_burst_test_sub(); 

                printk("[mt6332_upmu_set_vdvfs2_burst_sel(1)]\n");
                mt6332_upmu_set_vdvfs2_burst_sel(1);                
                exec_vdvfs2_burst_test_sub();

                printk("[mt6332_upmu_set_vdvfs2_burst_sel(2)]\n");
                mt6332_upmu_set_vdvfs2_burst_sel(2);
                exec_vdvfs2_burst_test_sub();

                printk("[mt6332_upmu_set_vdvfs2_burst_sel(3)]\n");
                mt6332_upmu_set_vdvfs2_burst_sel(3);
                exec_vdvfs2_burst_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vrf1_burst_test_sub_burst(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VRF1_BURST_SLEEP_MASK;k++) {
        mt6332_upmu_set_vrf1_burst_sleep(k);         
        printk("[exec_vrf1_burst_test_sub_burst] burst_sleep=%d, qi_burst=%d\n", k, mt6332_upmu_get_qi_vrf1_burst());
    }
    
    for(k=0;k<=MT6332_PMIC_VRF1_BURST_ON_MASK;k++) {
        mt6332_upmu_set_vrf1_burst_on(k);        
        printk("[exec_vrf1_burst_test_sub_burst] burst_on=%d, qi_burst=%d\n", k, mt6332_upmu_get_qi_vrf1_burst());
    }
}

void exec_vrf1_burst_test_sub(void)
{
    printk("[exec_vrf1_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vrf1_burst_test_sub_burst();

    printk("[exec_vrf1_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vrf1_burst_test_sub_burst();

    printk("[exec_vrf1_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vrf1_burst_test_sub_burst();

    printk("[exec_vrf1_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vrf1_burst_test_sub_burst();
}

void exec_vrf1_burst_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vrf1_burst_ctrl] %d\n", i);
        mt6332_upmu_set_vrf1_burst_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VRF1_BURST_MASK;k++) {
                    mt6332_upmu_set_vrf1_burst(k);                     
                    printk("[exec_vrf1_burst_test] burst=%d, qi_burst=%d\n", k, mt6332_upmu_get_qi_vrf1_burst());
                }                    
                break;

            case 1:
                printk("[mt6332_upmu_set_vrf1_burst_sel(0)]\n");
                mt6332_upmu_set_vrf1_burst_sel(0);
                exec_vrf1_burst_test_sub(); 

                printk("[mt6332_upmu_set_vrf1_burst_sel(1)]\n");
                mt6332_upmu_set_vrf1_burst_sel(1);                
                exec_vrf1_burst_test_sub();

                printk("[mt6332_upmu_set_vrf1_burst_sel(2)]\n");
                mt6332_upmu_set_vrf1_burst_sel(2);
                exec_vrf1_burst_test_sub();

                printk("[mt6332_upmu_set_vrf1_burst_sel(3)]\n");
                mt6332_upmu_set_vrf1_burst_sel(3);
                exec_vrf1_burst_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vrf2_burst_test_sub_burst(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VRF2_BURST_SLEEP_MASK;k++) {
        mt6332_upmu_set_vrf2_burst_sleep(k);         
        printk("[exec_vrf2_burst_test_sub_burst] burst_sleep=%d, qi_burst=%d\n", k, mt6332_upmu_get_qi_vrf2_burst());
    }
    
    for(k=0;k<=MT6332_PMIC_VRF2_BURST_ON_MASK;k++) {
        mt6332_upmu_set_vrf2_burst_on(k);        
        printk("[exec_vrf2_burst_test_sub_burst] burst_on=%d, qi_burst=%d\n", k, mt6332_upmu_get_qi_vrf2_burst());
    }
}

void exec_vrf2_burst_test_sub(void)
{
    printk("[exec_vrf2_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vrf2_burst_test_sub_burst();

    printk("[exec_vrf2_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vrf2_burst_test_sub_burst();

    printk("[exec_vrf2_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vrf2_burst_test_sub_burst();

    printk("[exec_vrf2_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vrf2_burst_test_sub_burst();
}

void exec_vrf2_burst_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vrf2_burst_ctrl] %d\n", i);
        mt6332_upmu_set_vrf2_burst_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VRF2_BURST_MASK;k++) {
                    mt6332_upmu_set_vrf2_burst(k);                     
                    printk("[exec_vrf2_burst_test] burst=%d, qi_burst=%d\n", k, mt6332_upmu_get_qi_vrf2_burst());
                }                    
                break;

            case 1:
                printk("[mt6332_upmu_set_vrf2_burst_sel(0)]\n");
                mt6332_upmu_set_vrf2_burst_sel(0);
                exec_vrf2_burst_test_sub(); 

                printk("[mt6332_upmu_set_vrf2_burst_sel(1)]\n");
                mt6332_upmu_set_vrf2_burst_sel(1);                
                exec_vrf2_burst_test_sub();

                printk("[mt6332_upmu_set_vrf2_burst_sel(2)]\n");
                mt6332_upmu_set_vrf2_burst_sel(2);
                exec_vrf2_burst_test_sub();

                printk("[mt6332_upmu_set_vrf2_burst_sel(3)]\n");
                mt6332_upmu_set_vrf2_burst_sel(3);
                exec_vrf2_burst_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vpa_burst_test_sub_burst(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VPA_BURSTH_SLEEP_MASK;k++) {
        mt6332_upmu_set_vpa_bursth_sleep(k);         
        mt6332_upmu_set_vpa_burstl_sleep(k);
        
        printk("[exec_vpa_burst_test_sub_burst] bursth_sleep=%d, burstl_sleep=%d, qi_bursth=%d, qi_burstl=%d\n", 
            k, k, mt6332_upmu_get_qi_vpa_bursth(), mt6332_upmu_get_qi_vpa_burstl());
    }
    
    for(k=0;k<=MT6332_PMIC_VPA_BURSTH_ON_MASK;k++) {
        mt6332_upmu_set_vpa_bursth_on(k);        
        mt6332_upmu_set_vpa_burstl_on(k);        
        
        printk("[exec_vpa_burst_test_sub_burst] bursth_on=%d, burstl_on=%d, qi_bursth=%d, qi_burstl=%d\n", 
            k, k, mt6332_upmu_get_qi_vpa_bursth(), mt6332_upmu_get_qi_vpa_burstl());
    }
}

void exec_vpa_burst_test_sub(void)
{
    printk("[exec_vpa_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vpa_burst_test_sub_burst();

    printk("[exec_vpa_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vpa_burst_test_sub_burst();

    printk("[exec_vpa_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vpa_burst_test_sub_burst();

    printk("[exec_vpa_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vpa_burst_test_sub_burst();
}

void exec_vpa_burst_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vpa_burst_ctrl] %d\n", i);
        mt6332_upmu_set_vpa_burst_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VPA_BURSTH_MASK;k++) {
                    mt6332_upmu_set_vpa_bursth(k);                     
                    mt6332_upmu_set_vpa_burstl(k);                     
                    
                    printk("[exec_vpa_burst_test] bursth=%d, burstl=%d, qi_bursth=%d, qi_burstl=%d\n", 
                        k, k, mt6332_upmu_get_qi_vpa_bursth(), mt6332_upmu_get_qi_vpa_burstl());
                }                    
                break;

            case 1:
                printk("[mt6332_upmu_set_vpa_burst_sel(0)]\n");
                mt6332_upmu_set_vpa_burst_sel(0);
                exec_vpa_burst_test_sub(); 

                printk("[mt6332_upmu_set_vpa_burst_sel(1)]\n");
                mt6332_upmu_set_vpa_burst_sel(1);                
                exec_vpa_burst_test_sub();

                printk("[mt6332_upmu_set_vpa_burst_sel(2)]\n");
                mt6332_upmu_set_vpa_burst_sel(2);
                exec_vpa_burst_test_sub();

                printk("[mt6332_upmu_set_vpa_burst_sel(3)]\n");
                mt6332_upmu_set_vpa_burst_sel(3);
                exec_vpa_burst_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_vsbst_burst_test_sub_burst(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_VSBST_BURSTH_SLEEP_MASK;k++) {
        mt6332_upmu_set_vsbst_bursth_sleep(k);         
        mt6332_upmu_set_vsbst_burstl_sleep(k);
        
        printk("[exec_vsbst_burst_test_sub_burst] bursth_sleep=%d, burstl_sleep=%d, qi_bursth=%d, qi_burstl=%d\n", 
            k, k, mt6332_upmu_get_qi_vsbst_bursth(), mt6332_upmu_get_qi_vsbst_burstl());
    }
    
    for(k=0;k<=MT6332_PMIC_VSBST_BURSTH_ON_MASK;k++) {
        mt6332_upmu_set_vsbst_bursth_on(k);        
        mt6332_upmu_set_vsbst_burstl_on(k);        
        
        printk("[exec_vsbst_burst_test_sub_burst] bursth_on=%d, burstl_on=%d, qi_bursth=%d, qi_burstl=%d\n", 
            k, k, mt6332_upmu_get_qi_vsbst_bursth(), mt6332_upmu_get_qi_vsbst_burstl());
    }
}

void exec_vsbst_burst_test_sub(void)
{
    printk("[exec_vsbst_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_vsbst_burst_test_sub_burst();

    printk("[exec_vsbst_burst_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_vsbst_burst_test_sub_burst();

    printk("[exec_vsbst_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_vsbst_burst_test_sub_burst();

    printk("[exec_vsbst_burst_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_vsbst_burst_test_sub_burst();
}

void exec_vsbst_burst_test(void)
{
    int i=0, k=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_vsbst_burst_ctrl] %d\n", i);
        mt6332_upmu_set_vsbst_burst_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_VSBST_BURSTH_MASK;k++) {
                    mt6332_upmu_set_vsbst_bursth(k);                     
                    mt6332_upmu_set_vsbst_burstl(k);                     
                    
                    printk("[exec_vsbst_burst_test] bursth=%d, burstl=%d, qi_bursth=%d, qi_burstl=%d\n", 
                        k, k, mt6332_upmu_get_qi_vsbst_bursth(), mt6332_upmu_get_qi_vsbst_burstl());
                }                    
                break;

            case 1:
                printk("[mt6332_upmu_set_vsbst_burst_sel(0)]\n");
                mt6332_upmu_set_vsbst_burst_sel(0);
                exec_vsbst_burst_test_sub(); 

                printk("[mt6332_upmu_set_vsbst_burst_sel(1)]\n");
                mt6332_upmu_set_vsbst_burst_sel(1);                
                exec_vsbst_burst_test_sub();

                printk("[mt6332_upmu_set_vsbst_burst_sel(2)]\n");
                mt6332_upmu_set_vsbst_burst_sel(2);
                exec_vsbst_burst_test_sub();

                printk("[mt6332_upmu_set_vsbst_burst_sel(3)]\n");
                mt6332_upmu_set_vsbst_burst_sel(3);
                exec_vsbst_burst_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void PMIC_BUCK_BURST(int index_val)
{
    printk("[PMIC_BUCK_BURST] start....\n");

    set_srclken_1_val(1); 
    set_srclken_2_val(1);
    set_srclken_sw_mode();

    //MT6331
    mt6331_upmu_set_vdvfs11_en_ctrl(0);
    mt6331_upmu_set_vdvfs12_en_ctrl(0);
    mt6331_upmu_set_vdvfs13_en_ctrl(0);
    mt6331_upmu_set_vdvfs14_en_ctrl(0);
    mt6331_upmu_set_vgpu_en_ctrl(0);
    mt6331_upmu_set_vcore1_en_ctrl(0);
    mt6331_upmu_set_vcore2_en_ctrl(0);
    mt6331_upmu_set_vio18_en_ctrl(0);

    //MT6332
    mt6332_upmu_set_vdram_en_ctrl(0);
    mt6332_upmu_set_vdvfs2_en_ctrl(0);
    mt6332_upmu_set_vrf1_en_ctrl(0);
    mt6332_upmu_set_vrf2_en_ctrl(0);
    mt6332_upmu_set_vpa_en_ctrl(0);
    mt6332_upmu_set_vsbst_en_ctrl(0);

    switch(index_val){
      //MT6331
      case 0 : printk("[PMIC_BUCK_BURST] no BURST function\n");              break;
      case 1 : printk("[PMIC_BUCK_BURST] no BURST function\n");              break;
      case 2 : printk("[PMIC_BUCK_BURST] no BURST function\n");              break;
      case 3 : printk("[PMIC_BUCK_BURST] no BURST function\n");              break;
      case 4 : mt6331_upmu_set_vgpu_en(1);    exec_vgpu_burst_test();      break;
      case 5 : mt6331_upmu_set_vcore1_en(1);  exec_vcore1_burst_test();    break;
      case 6 : mt6331_upmu_set_vcore2_en(1);  exec_vcore2_burst_test();    break;
      case 7 : mt6331_upmu_set_vio18_en(1);   exec_vio18_burst_test();     break;

      //MT6331
      case 8 : mt6332_upmu_set_vdram_en(1);   exec_vdram_burst_test();     break;
      case 9 : mt6332_upmu_set_vdvfs2_en(1);  exec_vdvfs2_burst_test();    break;
      case 10: mt6332_upmu_set_vrf1_en(1);    exec_vrf1_burst_test();      break;
      case 11: mt6332_upmu_set_vrf2_en(1);    exec_vrf2_burst_test();      break;
      case 12: mt6332_upmu_set_vpa_en(1);     exec_vpa_burst_test();       break;      
      case 13: mt6332_upmu_set_vpa_en(1);     exec_vsbst_burst_test();     break;

      default:
        printk("[PMIC_BUCK_BURST] Invalid channel value(%d)\n", index_val);
        break;
    }

    printk("[PMIC_BUCK_BURST] end....\n");
}
#endif

//////////////////////////////////////////
// LDO
//////////////////////////////////////////
#ifdef LDO_ON_OFF_DVT_ENABLE
void exec_6331_ldo_vtcxo1_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vtcxo1_on_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vtcxo1_on_ctrl(i);

        switch(i){
            case 0:
                printk("hwPowerOn(MT6331_POWER_LDO_VTCXO1]\n");
                        hwPowerOn(MT6331_POWER_LDO_VTCXO1, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VTCXO1]\n");
                        hwPowerDown(MT6331_POWER_LDO_VTCXO1, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerOn(MT6331_POWER_LDO_VTCXO1]\n");
                        hwPowerOn(MT6331_POWER_LDO_VTCXO1, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VTCXO1]\n");
                        hwPowerDown(MT6331_POWER_LDO_VTCXO1, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vtcxo1_srclk_en_sel(0)]\n");
                         mt6331_upmu_set_rg_vtcxo1_srclk_en_sel(0); exec_scrxxx_map(LDO_ONLY);                       

                printk("[mt6331_upmu_set_rg_vtcxo1_srclk_en_sel(1)]\n");
                         mt6331_upmu_set_rg_vtcxo1_srclk_en_sel(1); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vtcxo1_srclk_en_sel(2)]\n");
                         mt6331_upmu_set_rg_vtcxo1_srclk_en_sel(2); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vtcxo1_srclk_en_sel(3)]\n");
                         mt6331_upmu_set_rg_vtcxo1_srclk_en_sel(3); exec_scrxxx_map(LDO_ONLY);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vtcxo1_on_ctrl(0);\n"); mt6331_upmu_set_rg_vtcxo1_on_ctrl(0);
}

void exec_6331_ldo_vtcxo2_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vtcxo2_on_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vtcxo2_on_ctrl(i);

        switch(i){
            case 0:
                printk("hwPowerOn(MT6331_POWER_LDO_VTCXO2]\n");
                        hwPowerOn(MT6331_POWER_LDO_VTCXO2, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VTCXO2]\n");
                        hwPowerDown(MT6331_POWER_LDO_VTCXO2, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerOn(MT6331_POWER_LDO_VTCXO2]\n");
                        hwPowerOn(MT6331_POWER_LDO_VTCXO2, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VTCXO2]\n");
                        hwPowerDown(MT6331_POWER_LDO_VTCXO2, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vtcxo2_srclk_en_sel(0)]\n");
                         mt6331_upmu_set_rg_vtcxo2_srclk_en_sel(0); exec_scrxxx_map(LDO_ONLY);                       

                printk("[mt6331_upmu_set_rg_vtcxo2_srclk_en_sel(1)]\n");
                         mt6331_upmu_set_rg_vtcxo2_srclk_en_sel(1); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vtcxo2_srclk_en_sel(2)]\n");
                         mt6331_upmu_set_rg_vtcxo2_srclk_en_sel(2); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vtcxo2_srclk_en_sel(3)]\n");
                         mt6331_upmu_set_rg_vtcxo2_srclk_en_sel(3); exec_scrxxx_map(LDO_ONLY);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vtcxo2_on_ctrl(0);\n"); mt6331_upmu_set_rg_vtcxo2_on_ctrl(0);
}

void exec_6331_ldo_vaud32_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vaud32_on_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vaud32_on_ctrl(i);

        switch(i){
            case 0:
                printk("hwPowerOn(MT6331_POWER_LDO_VAUD32]\n");
                        hwPowerOn(MT6331_POWER_LDO_VAUD32, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VAUD32]\n");
                        hwPowerDown(MT6331_POWER_LDO_VAUD32, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerOn(MT6331_POWER_LDO_VAUD32]\n");
                        hwPowerOn(MT6331_POWER_LDO_VAUD32, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VAUD32]\n");
                        hwPowerDown(MT6331_POWER_LDO_VAUD32, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vaud32_srclk_en_sel(0)]\n");
                         mt6331_upmu_set_rg_vaud32_srclk_en_sel(0); exec_scrxxx_map(LDO_ONLY);                       

                printk("[mt6331_upmu_set_rg_vaud32_srclk_en_sel(1)]\n");
                         mt6331_upmu_set_rg_vaud32_srclk_en_sel(1); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vaud32_srclk_en_sel(2)]\n");
                         mt6331_upmu_set_rg_vaud32_srclk_en_sel(2); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vaud32_srclk_en_sel(3)]\n");
                         mt6331_upmu_set_rg_vaud32_srclk_en_sel(3); exec_scrxxx_map(LDO_ONLY);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vaud32_on_ctrl(0);\n"); mt6331_upmu_set_rg_vaud32_on_ctrl(0);
}

void exec_6331_ldo_vauxa32_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vauxa32_on_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vauxa32_on_ctrl(i);

        switch(i){
            case 0:
                printk("hwPowerOn(MT6331_POWER_LDO_VAUXA32]\n");
                        hwPowerOn(MT6331_POWER_LDO_VAUXA32, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VAUXA32]\n");
                        hwPowerDown(MT6331_POWER_LDO_VAUXA32, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerOn(MT6331_POWER_LDO_VAUXA32]\n");
                        hwPowerOn(MT6331_POWER_LDO_VAUXA32, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VAUXA32]\n");
                        hwPowerDown(MT6331_POWER_LDO_VAUXA32, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vauxa32_srclk_en_sel(0)]\n");
                         mt6331_upmu_set_rg_vauxa32_srclk_en_sel(0); exec_scrxxx_map(LDO_ONLY);                       

                printk("[mt6331_upmu_set_rg_vauxa32_srclk_en_sel(1)]\n");
                         mt6331_upmu_set_rg_vauxa32_srclk_en_sel(1); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vauxa32_srclk_en_sel(2)]\n");
                         mt6331_upmu_set_rg_vauxa32_srclk_en_sel(2); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vauxa32_srclk_en_sel(3)]\n");
                         mt6331_upmu_set_rg_vauxa32_srclk_en_sel(3); exec_scrxxx_map(LDO_ONLY);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vauxa32_on_ctrl(0);\n"); mt6331_upmu_set_rg_vauxa32_on_ctrl(0);
}

void exec_6331_ldo_vcama_en_test(void)
{
    printk("hwPowerOn(MT6331_POWER_LDO_VCAMA]\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMA, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VCAMA]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMA, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VCAMA]\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMA, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VCAMA]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMA, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
}

void exec_6331_ldo_vio28_en_test(void)
{
    printk("hwPowerOn(MT6331_POWER_LDO_VIO28]\n");
            hwPowerOn(MT6331_POWER_LDO_VIO28, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VIO28]\n");
            hwPowerDown(MT6331_POWER_LDO_VIO28, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VIO28]\n");
            hwPowerOn(MT6331_POWER_LDO_VIO28, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VIO28]\n");
            hwPowerDown(MT6331_POWER_LDO_VIO28, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
}

void exec_6331_ldo_vcam_af_en_test(void)
{
    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_AF]\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_AF, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_AF]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_AF, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_AF]\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_AF, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_AF]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_AF, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
}

void exec_6331_ldo_vmc_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vmc_on_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vmc_on_ctrl(i);

        switch(i){
            case 0:
                printk("hwPowerOn(MT6331_POWER_LDO_VMC]\n");
                        hwPowerOn(MT6331_POWER_LDO_VMC, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VMC]\n");
                        hwPowerDown(MT6331_POWER_LDO_VMC, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerOn(MT6331_POWER_LDO_VMC]\n");
                        hwPowerOn(MT6331_POWER_LDO_VMC, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VMC]\n");
                        hwPowerDown(MT6331_POWER_LDO_VMC, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vmc_srclk_en_sel(0)]\n");
                         mt6331_upmu_set_rg_vmc_srclk_en_sel(0); exec_scrxxx_map(LDO_ONLY);                       

                printk("[mt6331_upmu_set_rg_vmc_srclk_en_sel(1)]\n");
                         mt6331_upmu_set_rg_vmc_srclk_en_sel(1); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vmc_srclk_en_sel(2)]\n");
                         mt6331_upmu_set_rg_vmc_srclk_en_sel(2); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vmc_srclk_en_sel(3)]\n");
                         mt6331_upmu_set_rg_vmc_srclk_en_sel(3); exec_scrxxx_map(LDO_ONLY);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vmc_on_ctrl(0);\n"); mt6331_upmu_set_rg_vmc_on_ctrl(0);
}

void exec_6331_ldo_vmch_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vmch_on_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vmch_on_ctrl(i);

        switch(i){
            case 0:
                printk("hwPowerOn(MT6331_POWER_LDO_VMCH]\n");
                        hwPowerOn(MT6331_POWER_LDO_VMCH, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VMCH]\n");
                        hwPowerDown(MT6331_POWER_LDO_VMCH, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerOn(MT6331_POWER_LDO_VMCH]\n");
                        hwPowerOn(MT6331_POWER_LDO_VMCH, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VMCH]\n");
                        hwPowerDown(MT6331_POWER_LDO_VMCH, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vmch_srclk_en_sel(0)]\n");
                         mt6331_upmu_set_rg_vmch_srclk_en_sel(0); exec_scrxxx_map(LDO_ONLY);                       

                printk("[mt6331_upmu_set_rg_vmch_srclk_en_sel(1)]\n");
                         mt6331_upmu_set_rg_vmch_srclk_en_sel(1); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vmch_srclk_en_sel(2)]\n");
                         mt6331_upmu_set_rg_vmch_srclk_en_sel(2); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vmch_srclk_en_sel(3)]\n");
                         mt6331_upmu_set_rg_vmch_srclk_en_sel(3); exec_scrxxx_map(LDO_ONLY);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vmch_on_ctrl(0);\n"); mt6331_upmu_set_rg_vmch_on_ctrl(0);
}

void exec_6331_ldo_vemc33_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vemc33_on_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vemc33_on_ctrl(i);

        switch(i){
            case 0:
                printk("hwPowerOn(MT6331_POWER_LDO_VEMC33]\n");
                        hwPowerOn(MT6331_POWER_LDO_VEMC33, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VEMC33]\n");
                        hwPowerDown(MT6331_POWER_LDO_VEMC33, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerOn(MT6331_POWER_LDO_VEMC33]\n");
                        hwPowerOn(MT6331_POWER_LDO_VEMC33, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VEMC33]\n");
                        hwPowerDown(MT6331_POWER_LDO_VEMC33, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vemc33_srclk_en_sel(0)]\n");
                         mt6331_upmu_set_rg_vemc33_srclk_en_sel(0); exec_scrxxx_map(LDO_ONLY);                       

                printk("[mt6331_upmu_set_rg_vemc33_srclk_en_sel(1)]\n");
                         mt6331_upmu_set_rg_vemc33_srclk_en_sel(1); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vemc33_srclk_en_sel(2)]\n");
                         mt6331_upmu_set_rg_vemc33_srclk_en_sel(2); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vemc33_srclk_en_sel(3)]\n");
                         mt6331_upmu_set_rg_vemc33_srclk_en_sel(3); exec_scrxxx_map(LDO_ONLY);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vemc33_on_ctrl(0);\n"); mt6331_upmu_set_rg_vemc33_on_ctrl(0);
}

void exec_6331_ldo_vgp1_en_test(void)
{
    printk("hwPowerOn(MT6331_POWER_LDO_VGP1]\n");
            hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VGP1]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP1, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VGP1]\n");
            hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VGP1]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP1, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
}

void exec_6331_ldo_vgp4_en_test(void)
{
    printk("hwPowerOn(MT6331_POWER_LDO_VGP4]\n");
            hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VGP4]\n");
            hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
}

void exec_6331_ldo_vsim1_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vsim1_on_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vsim1_on_ctrl(i);

        switch(i){
            case 0:
                printk("hwPowerOn(MT6331_POWER_LDO_VSIM1]\n");
                        hwPowerOn(MT6331_POWER_LDO_VSIM1, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VSIM1]\n");
                        hwPowerDown(MT6331_POWER_LDO_VSIM1, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerOn(MT6331_POWER_LDO_VSIM1]\n");
                        hwPowerOn(MT6331_POWER_LDO_VSIM1, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VSIM1]\n");
                        hwPowerDown(MT6331_POWER_LDO_VSIM1, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vsim1_srclk_en_sel(0)]\n");
                         mt6331_upmu_set_rg_vsim1_srclk_en_sel(0); exec_scrxxx_map(LDO_ONLY);                       

                printk("[mt6331_upmu_set_rg_vsim1_srclk_en_sel(1)]\n");
                         mt6331_upmu_set_rg_vsim1_srclk_en_sel(1); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vsim1_srclk_en_sel(2)]\n");
                         mt6331_upmu_set_rg_vsim1_srclk_en_sel(2); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vsim1_srclk_en_sel(3)]\n");
                         mt6331_upmu_set_rg_vsim1_srclk_en_sel(3); exec_scrxxx_map(LDO_ONLY);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vsim1_on_ctrl(0);\n"); mt6331_upmu_set_rg_vsim1_on_ctrl(0);
}

void exec_6331_ldo_vsim2_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vsim2_on_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vsim2_on_ctrl(i);

        switch(i){
            case 0:
                printk("hwPowerOn(MT6331_POWER_LDO_VSIM2]\n");
                        hwPowerOn(MT6331_POWER_LDO_VSIM2, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VSIM2]\n");
                        hwPowerDown(MT6331_POWER_LDO_VSIM2, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerOn(MT6331_POWER_LDO_VSIM2]\n");
                        hwPowerOn(MT6331_POWER_LDO_VSIM2, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VSIM2]\n");
                        hwPowerDown(MT6331_POWER_LDO_VSIM2, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vsim2_srclk_en_sel(0)]\n");
                         mt6331_upmu_set_rg_vsim2_srclk_en_sel(0); exec_scrxxx_map(LDO_ONLY);                       

                printk("[mt6331_upmu_set_rg_vsim2_srclk_en_sel(1)]\n");
                         mt6331_upmu_set_rg_vsim2_srclk_en_sel(1); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vsim2_srclk_en_sel(2)]\n");
                         mt6331_upmu_set_rg_vsim2_srclk_en_sel(2); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vsim2_srclk_en_sel(3)]\n");
                         mt6331_upmu_set_rg_vsim2_srclk_en_sel(3); exec_scrxxx_map(LDO_ONLY);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vsim2_on_ctrl(0);\n"); mt6331_upmu_set_rg_vsim2_on_ctrl(0);
}

void exec_6331_ldo_vfbb_en_test(void)
{
    printk("hwPowerOn(MT6331_POWER_LDO_VFBB]\n");
            hwPowerOn(MT6331_POWER_LDO_VFBB, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VFBB]\n");
            hwPowerDown(MT6331_POWER_LDO_VFBB, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VFBB]\n");
            hwPowerOn(MT6331_POWER_LDO_VFBB, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VFBB]\n");
            hwPowerDown(MT6331_POWER_LDO_VFBB, "ldo_dvt"); read_adc_value(LDO_ONLY);                       
}

void exec_6331_ldo_vibr_en_test(void)
{
    printk("hwPowerOn(MT6331_POWER_LDO_VIBR]\n");
            hwPowerOn(MT6331_POWER_LDO_VIBR, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VIBR]\n");
            hwPowerDown(MT6331_POWER_LDO_VIBR, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VIBR]\n");
            hwPowerOn(MT6331_POWER_LDO_VIBR, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VIBR]\n");
            hwPowerDown(MT6331_POWER_LDO_VIBR, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
}

void exec_6331_ldo_vrtc_en_test(void)
{
    mt6331_upmu_set_rg_vrtc_force_on(0);

    printk("hwPowerOn(MT6331_POWER_LDO_VRTC]\n");
            hwPowerOn(MT6331_POWER_LDO_VRTC, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VRTC]\n");
            hwPowerDown(MT6331_POWER_LDO_VRTC, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VRTC]\n");
            hwPowerOn(MT6331_POWER_LDO_VRTC, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VRTC]\n");
            hwPowerDown(MT6331_POWER_LDO_VRTC, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
}

void exec_6331_ldo_vdig18_en_test(void)
{
    printk("exec_6331_ldo_vdig18_en_test : HW no en bit\n");                      
}

void exec_6331_ldo_vmipi_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vmipi_on_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vmipi_on_ctrl(i);

        switch(i){
            case 0:
                printk("hwPowerOn(MT6331_POWER_LDO_VMIPI\n");
                        hwPowerOn(MT6331_POWER_LDO_VMIPI, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VMIPI]\n");
                        hwPowerDown(MT6331_POWER_LDO_VMIPI, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerOn(MT6331_POWER_LDO_VMIPI]\n");
                        hwPowerOn(MT6331_POWER_LDO_VMIPI, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VMIPI]\n");
                        hwPowerDown(MT6331_POWER_LDO_VMIPI, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vmipi_srclk_en_sel(0)]\n");
                         mt6331_upmu_set_rg_vmipi_srclk_en_sel(0); exec_scrxxx_map(LDO_ONLY);                       

                printk("[mt6331_upmu_set_rg_vmipi_srclk_en_sel(1)]\n");
                         mt6331_upmu_set_rg_vmipi_srclk_en_sel(1); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vmipi_srclk_en_sel(2)]\n");
                         mt6331_upmu_set_rg_vmipi_srclk_en_sel(2); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vmipi_srclk_en_sel(3)]\n");
                         mt6331_upmu_set_rg_vmipi_srclk_en_sel(3); exec_scrxxx_map(LDO_ONLY);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vmipi_on_ctrl(0);\n"); mt6331_upmu_set_rg_vmipi_on_ctrl(0);
}

void exec_6331_ldo_vcamd_en_test(void)
{
    printk("hwPowerOn(MT6331_POWER_LDO_VCAMD]\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMD, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VCAMD]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMD, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VCAMD]\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMD, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VCAMD]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMD, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
}

void exec_6331_ldo_vusb10_en_test(void)
{
    printk("hwPowerOn(MT6331_POWER_LDO_VUSB10]\n");
            hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VUSB10]\n");
            hwPowerDown(MT6331_POWER_LDO_VUSB10, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VUSB10]\n");
            hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VUSB10]\n");
            hwPowerDown(MT6331_POWER_LDO_VUSB10, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
}

void exec_6331_ldo_vcam_io_en_test(void)
{
    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_IO]\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_IO, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_IO]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_IO, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_IO]\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_IO, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_IO]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_IO, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
}

void exec_6331_ldo_vsram_dvfs1_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vsram_dvfs1_on_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vsram_dvfs1_on_ctrl(i);

        switch(i){
            case 0:
                printk("hwPowerOn(MT6331_POWER_LDO_VSRAM_DVFS1]\n");
                        hwPowerOn(MT6331_POWER_LDO_VSRAM_DVFS1, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VSRAM_DVFS1]\n");
                        hwPowerDown(MT6331_POWER_LDO_VSRAM_DVFS1, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerOn(MT6331_POWER_LDO_VSRAM_DVFS1]\n");
                        hwPowerOn(MT6331_POWER_LDO_VSRAM_DVFS1, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6331_POWER_LDO_VSRAM_DVFS1]\n");
                        hwPowerDown(MT6331_POWER_LDO_VSRAM_DVFS1, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vsram_dvfs1_srclk_en_sel(0)]\n");
                         mt6331_upmu_set_rg_vsram_dvfs1_srclk_en_sel(0); exec_scrxxx_map(LDO_ONLY);                       

                printk("[mt6331_upmu_set_rg_vsram_dvfs1_srclk_en_sel(1)]\n");
                         mt6331_upmu_set_rg_vsram_dvfs1_srclk_en_sel(1); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vsram_dvfs1_srclk_en_sel(2)]\n");
                         mt6331_upmu_set_rg_vsram_dvfs1_srclk_en_sel(2); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6331_upmu_set_rg_vsram_dvfs1_srclk_en_sel(3)]\n");
                         mt6331_upmu_set_rg_vsram_dvfs1_srclk_en_sel(3); exec_scrxxx_map(LDO_ONLY);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vsram_dvfs1_on_ctrl(0);\n"); mt6331_upmu_set_rg_vsram_dvfs1_on_ctrl(0);
}

void exec_6331_ldo_vgp2_en_test(void)
{
    printk("hwPowerOn(MT6331_POWER_LDO_VGP2]\n");
            hwPowerOn(MT6331_POWER_LDO_VGP2, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VGP2]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP2, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VGP2]\n");
            hwPowerOn(MT6331_POWER_LDO_VGP2, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VGP2]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP2, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
}

void exec_6331_ldo_vgp3_en_test(void)
{
    printk("hwPowerOn(MT6331_POWER_LDO_VGP3]\n");
            hwPowerOn(MT6331_POWER_LDO_VGP3, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VGP3]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP3, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VGP3]\n");
            hwPowerOn(MT6331_POWER_LDO_VGP3, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VGP3]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP3, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
}

void exec_6331_ldo_vbiasn_en_test(void)
{
    printk("hwPowerOn(MT6331_POWER_LDO_VBIASN]\n");
            hwPowerOn(MT6331_POWER_LDO_VBIASN, VOL_0500, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VBIASN]\n");
            hwPowerDown(MT6331_POWER_LDO_VBIASN, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VBIASN]\n");
            hwPowerOn(MT6331_POWER_LDO_VBIASN, VOL_0500, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6331_POWER_LDO_VBIASN]\n");
            hwPowerDown(MT6331_POWER_LDO_VBIASN, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
}

void exec_6332_ldo_vauxb32_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_rg_vauxb32_on_ctrl] %d\n", i);
        mt6332_upmu_set_rg_vauxb32_on_ctrl(i);

        switch(i){
            case 0:
                printk("hwPowerOn(MT6332_POWER_LDO_VAUXB32]\n");
                        hwPowerOn(MT6332_POWER_LDO_VAUXB32, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6332_POWER_LDO_VAUXB32]\n");
                        hwPowerDown(MT6332_POWER_LDO_VAUXB32, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerOn(MT6332_POWER_LDO_VAUXB32]\n");
                        hwPowerOn(MT6332_POWER_LDO_VAUXB32, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6332_POWER_LDO_VAUXB32]\n");
                        hwPowerDown(MT6332_POWER_LDO_VAUXB32, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
                break;

            case 1:
                printk("[mt6332_upmu_set_rg_vauxb32_srclk_en_sel(0)]\n");
                         mt6332_upmu_set_rg_vauxb32_srclk_en_sel(0); exec_scrxxx_map(LDO_ONLY);                       

                printk("[mt6332_upmu_set_rg_vauxb32_srclk_en_sel(1)]\n");
                         mt6332_upmu_set_rg_vauxb32_srclk_en_sel(1); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6332_upmu_set_rg_vauxb32_srclk_en_sel(2)]\n");
                         mt6332_upmu_set_rg_vauxb32_srclk_en_sel(2); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6332_upmu_set_rg_vauxb32_srclk_en_sel(3)]\n");
                         mt6332_upmu_set_rg_vauxb32_srclk_en_sel(3); exec_scrxxx_map(LDO_ONLY);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6332_upmu_set_rg_vauxb32_on_ctrl(0);\n"); mt6332_upmu_set_rg_vauxb32_on_ctrl(0);
}

void exec_6332_ldo_vbif28_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_rg_vbif28_on_ctrl] %d\n", i);
        mt6332_upmu_set_rg_vbif28_on_ctrl(i);

        switch(i){
            case 0:
                printk("hwPowerOn(MT6332_POWER_LDO_VBIF28]\n");
                        hwPowerOn(MT6332_POWER_LDO_VBIF28, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6332_POWER_LDO_VBIF28]\n");
                        hwPowerDown(MT6332_POWER_LDO_VBIF28, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerOn(MT6332_POWER_LDO_VBIF28]\n");
                        hwPowerOn(MT6332_POWER_LDO_VBIF28, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6332_POWER_LDO_VBIF28]\n");
                        hwPowerDown(MT6332_POWER_LDO_VBIF28, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
                break;

            case 1:
                printk("[mt6332_upmu_set_rg_vbif28_srclk_en_sel(0)]\n");
                         mt6332_upmu_set_rg_vbif28_srclk_en_sel(0); exec_scrxxx_map(LDO_ONLY);                       

                printk("[mt6332_upmu_set_rg_vbif28_srclk_en_sel(1)]\n");
                         mt6332_upmu_set_rg_vbif28_srclk_en_sel(1); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6332_upmu_set_rg_vbif28_srclk_en_sel(2)]\n");
                         mt6332_upmu_set_rg_vbif28_srclk_en_sel(2); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6332_upmu_set_rg_vbif28_srclk_en_sel(3)]\n");
                         mt6332_upmu_set_rg_vbif28_srclk_en_sel(3); exec_scrxxx_map(LDO_ONLY);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6332_upmu_set_rg_vbif28_on_ctrl(0);\n"); mt6332_upmu_set_rg_vbif28_on_ctrl(0);
}

void exec_6332_ldo_vusb33_en_test(void)
{
    printk("hwPowerOn(MT6332_POWER_LDO_VUSB33]\n");
            hwPowerOn(MT6332_POWER_LDO_VUSB33, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6332_POWER_LDO_VUSB33]\n");
            hwPowerDown(MT6332_POWER_LDO_VUSB33, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6332_POWER_LDO_VUSB33]\n");
            hwPowerOn(MT6332_POWER_LDO_VUSB33, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerDown(MT6332_POWER_LDO_VUSB33]\n");
            hwPowerDown(MT6332_POWER_LDO_VUSB33, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
}

void exec_6332_ldo_vdig18_en_test(void)
{
    printk("exec_6332_ldo_vdig18_en_test : HW no en bit\n");                        
}

void exec_6332_ldo_vsram_dvfs2_en_test(void)
{
    int i=0;

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_rg_vsram_dvfs2_on_ctrl] %d\n", i);
        mt6332_upmu_set_rg_vsram_dvfs2_on_ctrl(i);

        switch(i){
            case 0:
                printk("hwPowerOn(MT6332_POWER_LDO_VSRAM_DVFS2]\n");
                        hwPowerOn(MT6332_POWER_LDO_VSRAM_DVFS2, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6332_POWER_LDO_VSRAM_DVFS2]\n");
                        hwPowerDown(MT6332_POWER_LDO_VSRAM_DVFS2, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerOn(MT6332_POWER_LDO_VSRAM_DVFS2]\n");
                        hwPowerOn(MT6332_POWER_LDO_VSRAM_DVFS2, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);

                printk("hwPowerDown(MT6332_POWER_LDO_VSRAM_DVFS2]\n");
                        hwPowerDown(MT6332_POWER_LDO_VSRAM_DVFS2, "ldo_dvt"); read_adc_value(LDO_ONLY);                        
                break;

            case 1:
                printk("[mt6332_upmu_set_rg_vsram_dvfs2_srclk_en_sel(0)]\n");
                         mt6332_upmu_set_rg_vsram_dvfs2_srclk_en_sel(0); exec_scrxxx_map(LDO_ONLY);                       

                printk("[mt6332_upmu_set_rg_vsram_dvfs2_srclk_en_sel(1)]\n");
                         mt6332_upmu_set_rg_vsram_dvfs2_srclk_en_sel(1); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6332_upmu_set_rg_vsram_dvfs2_srclk_en_sel(2)]\n");
                         mt6332_upmu_set_rg_vsram_dvfs2_srclk_en_sel(2); exec_scrxxx_map(LDO_ONLY);

                printk("[mt6332_upmu_set_rg_vsram_dvfs2_srclk_en_sel(3)]\n");
                         mt6332_upmu_set_rg_vsram_dvfs2_srclk_en_sel(3); exec_scrxxx_map(LDO_ONLY);
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6332_upmu_set_rg_vsram_dvfs2_on_ctrl(0);\n"); mt6332_upmu_set_rg_vsram_dvfs2_on_ctrl(0);
}
#endif

#ifdef LDO_VOSEL_DVT_ENABLE
void exec_6331_ldo_vtcxo1_vosel_test(void)
{
    int i=0;
    
    mt6331_upmu_set_rg_vtcxo1_on_ctrl(0);
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VTCXO1] 2800\n");
            hwPowerOn(MT6331_POWER_LDO_VTCXO1, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VTCXO1]\n");
            hwPowerDown(MT6331_POWER_LDO_VTCXO1, "ldo_dvt"); read_adc_value(ADC_ONLY);
    //---------------------------------------------------------------------------
    mt6331_upmu_set_rg_vtcxo1_en(1);
    for(i=0;i<=3;i++)
    {
        printk("mt6331_upmu_set_rg_vtcxo1_vosel=%d\n", i);
        mt6331_upmu_set_rg_vtcxo1_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vtcxo2_vosel_test(void)
{
    int i=0;
        
    mt6331_upmu_set_rg_vtcxo2_on_ctrl(0);
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VTCXO2] 2800\n");
            hwPowerOn(MT6331_POWER_LDO_VTCXO2, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VTCXO2]\n");
            hwPowerDown(MT6331_POWER_LDO_VTCXO2, "ldo_dvt"); read_adc_value(ADC_ONLY);
    //---------------------------------------------------------------------------
    mt6331_upmu_set_rg_vtcxo2_en(1);
    for(i=0;i<=3;i++)
    {
        printk("mt6331_upmu_set_rg_vtcxo2_vosel=%d\n", i);
        mt6331_upmu_set_rg_vtcxo2_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vaud32_vosel_test(void)
{
    int i=0; 
    
    mt6331_upmu_set_rg_vaud32_on_ctrl(0);
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VAUD32] VOL_2800\n");
            hwPowerOn(MT6331_POWER_LDO_VAUD32, VOL_2800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VAUD32]\n");
            hwPowerDown(MT6331_POWER_LDO_VAUD32, "ldo_dvt"); read_adc_value(ADC_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VAUD32] VOL_3000\n");
            hwPowerOn(MT6331_POWER_LDO_VAUD32, VOL_3000, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VAUD32]\n");
            hwPowerDown(MT6331_POWER_LDO_VAUD32, "ldo_dvt"); read_adc_value(ADC_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VAUD32] VOL_3200\n");
            hwPowerOn(MT6331_POWER_LDO_VAUD32, VOL_3200, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VAUD32]\n");
            hwPowerDown(MT6331_POWER_LDO_VAUD32, "ldo_dvt"); read_adc_value(ADC_ONLY);
    //---------------------------------------------------------------------------
    mt6331_upmu_set_rg_vaud32_en(1);
    for(i=0;i<=3;i++)
    {
        printk("mt6331_upmu_set_rg_vaud32_vosel=%d\n", i);
        mt6331_upmu_set_rg_vaud32_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vauxa32_vosel_test(void)
{
    int i=0;
    
    mt6331_upmu_set_rg_vauxa32_on_ctrl(0);
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VAUXA32] VOL_2800\n");
            hwPowerOn(MT6331_POWER_LDO_VAUXA32, VOL_2800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VAUXA32]\n");
            hwPowerDown(MT6331_POWER_LDO_VAUXA32, "ldo_dvt"); read_adc_value(ADC_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VAUXA32] VOL_3000\n");
            hwPowerOn(MT6331_POWER_LDO_VAUXA32, VOL_3000, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VAUXA32]\n");
            hwPowerDown(MT6331_POWER_LDO_VAUXA32, "ldo_dvt"); read_adc_value(ADC_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VAUXA32] VOL_3200\n");
            hwPowerOn(MT6331_POWER_LDO_VAUXA32, VOL_3200, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VAUXA32]\n");
            hwPowerDown(MT6331_POWER_LDO_VAUXA32, "ldo_dvt"); read_adc_value(ADC_ONLY);        
    //---------------------------------------------------------------------------
    mt6331_upmu_set_rg_vauxa32_en(1);
    for(i=0;i<=3;i++)
    {
        printk("mt6331_upmu_set_rg_vauxa32_vosel=%d\n", i);
        mt6331_upmu_set_rg_vauxa32_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vcama_vosel_test(void)
{   
    int i=0;
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VCAMA] VOL_1500\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMA, VOL_1500, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAMA]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMA, "ldo_dvt"); read_adc_value(ADC_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VCAMA] VOL_1800\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMA, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAMA]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMA, "ldo_dvt"); read_adc_value(ADC_ONLY);  

    printk("hwPowerOn(MT6331_POWER_LDO_VCAMA] VOL_2500\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMA, VOL_2500, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAMA]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMA, "ldo_dvt"); read_adc_value(ADC_ONLY);  
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAMA] VOL_2800\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMA, VOL_2800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAMA]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMA, "ldo_dvt"); read_adc_value(ADC_ONLY);
    //---------------------------------------------------------------------------
    mt6331_upmu_set_rg_vcama_en(1);
    for(i=0;i<=7;i++)
    {
        printk("mt6331_upmu_set_rg_vcama_vosel=%d\n", i);
        mt6331_upmu_set_rg_vcama_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vio28_vosel_test(void)
{   
    int i=0;
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VIO28] VOL_DEFAULT\n");
            hwPowerOn(MT6331_POWER_LDO_VIO28, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VIO28]\n");
            hwPowerDown(MT6331_POWER_LDO_VIO28, "ldo_dvt"); read_adc_value(ADC_ONLY);
    //---------------------------------------------------------------------------        
    mt6331_upmu_set_rg_vio28_en(1);
    for(i=0;i<=7;i++)
    {
        printk("mt6331_upmu_set_rg_vio28_vosel=%d\n", i);
        mt6331_upmu_set_rg_vio28_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vcam_af_vosel_test(void)
{   
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_AF] VOL_1200\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_AF, VOL_1200, "ldo_dvt"); read_adc_value(ADC_ONLY);            
    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_AF]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_AF, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_AF] VOL_1300\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_AF, VOL_1300, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_AF]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_AF, "ldo_dvt"); read_adc_value(ADC_ONLY);                   
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_AF] VOL_1500\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_AF, VOL_1500, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_AF]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_AF, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_AF] VOL_1800\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_AF, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_AF]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_AF, "ldo_dvt"); read_adc_value(ADC_ONLY);                        
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_AF] VOL_2000\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_AF, VOL_2000, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_AF]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_AF, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_AF] VOL_2800\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_AF, VOL_2800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_AF]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_AF, "ldo_dvt"); read_adc_value(ADC_ONLY);  
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_AF] VOL_3000\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_AF, VOL_3000, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_AF]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_AF, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_AF] VOL_3300\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_AF, VOL_3300, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_AF]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_AF, "ldo_dvt"); read_adc_value(ADC_ONLY);            
    //---------------------------------------------------------------------------        
}

void exec_6331_ldo_vmc_vosel_test(void)
{
    int i=0;
    
    mt6331_upmu_set_rg_vmc_on_ctrl(0);
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VMC] VOL_1800\n");
            hwPowerOn(MT6331_POWER_LDO_VMC, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VMC]\n");
            hwPowerDown(MT6331_POWER_LDO_VMC, "ldo_dvt"); read_adc_value(ADC_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VMC] VOL_3300\n");
            hwPowerOn(MT6331_POWER_LDO_VMC, VOL_3300, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VMC]\n");
            hwPowerDown(MT6331_POWER_LDO_VMC, "ldo_dvt"); read_adc_value(ADC_ONLY);
    //---------------------------------------------------------------------------        
    mt6331_upmu_set_rg_vmc_en(1);
    for(i=0;i<=7;i++)
    {
        printk("mt6331_upmu_set_rg_vmc_vosel=%d\n", i);
        mt6331_upmu_set_rg_vmc_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vmch_vosel_test(void)
{
    mt6331_upmu_set_rg_vmch_on_ctrl(0);
    
    printk("hwPowerOn(MT6331_POWER_LDO_VMCH] VOL_3000\n");
            hwPowerOn(MT6331_POWER_LDO_VMCH, VOL_3000, "ldo_dvt"); read_adc_value(LDO_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VMCH]\n");
            hwPowerDown(MT6331_POWER_LDO_VMCH, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VMCH] VOL_3300\n");
            hwPowerOn(MT6331_POWER_LDO_VMCH, VOL_3300, "ldo_dvt"); read_adc_value(LDO_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VMCH]\n");
            hwPowerDown(MT6331_POWER_LDO_VMCH, "ldo_dvt"); read_adc_value(LDO_ONLY);        
}

void exec_6331_ldo_vemc33_vosel_test(void)
{
    mt6331_upmu_set_rg_vemc33_on_ctrl(0);
    
    printk("hwPowerOn(MT6331_POWER_LDO_VEMC33] VOL_3000\n");
            hwPowerOn(MT6331_POWER_LDO_VEMC33, VOL_3000, "ldo_dvt"); read_adc_value(LDO_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VEMC33]\n");
            hwPowerDown(MT6331_POWER_LDO_VEMC33, "ldo_dvt"); read_adc_value(LDO_ONLY);

    printk("hwPowerOn(MT6331_POWER_LDO_VEMC33] VOL_3300\n");
            hwPowerOn(MT6331_POWER_LDO_VEMC33, VOL_3300, "ldo_dvt"); read_adc_value(LDO_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VEMC33]\n");
            hwPowerDown(MT6331_POWER_LDO_VEMC33, "ldo_dvt"); read_adc_value(LDO_ONLY);        
}

void exec_6331_ldo_vgp1_vosel_test(void)
{   
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VGP1] VOL_1200\n");
            hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_1200, "ldo_dvt"); read_adc_value(ADC_ONLY);            
    printk("hwPowerDown(MT6331_POWER_LDO_VGP1]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP1, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VGP1] VOL_1300\n");
            hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_1300, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VGP1]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP1, "ldo_dvt"); read_adc_value(ADC_ONLY);                   
            
    printk("hwPowerOn(MT6331_POWER_LDO_VGP1] VOL_1500\n");
            hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_1500, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VGP1]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP1, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VGP1] VOL_1800\n");
            hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VGP1]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP1, "ldo_dvt"); read_adc_value(ADC_ONLY);                        
            
    printk("hwPowerOn(MT6331_POWER_LDO_VGP1] VOL_2000\n");
            hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_2000, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VGP1]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP1, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VGP1] VOL_2800\n");
            hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_2800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VGP1]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP1, "ldo_dvt"); read_adc_value(ADC_ONLY);  
            
    printk("hwPowerOn(MT6331_POWER_LDO_VGP1] VOL_3000\n");
            hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_3000, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VGP1]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP1, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VGP1] VOL_3300\n");
            hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_3300, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VGP1]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP1, "ldo_dvt"); read_adc_value(ADC_ONLY);            
    //---------------------------------------------------------------------------        
}

void exec_6331_ldo_vgp4_vosel_test(void)
{   
    if(get_pmic_mt6331_cid()==PMIC6331_E1_CID_CODE)
    {
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] MT6331==E1\n");
        
        //---------------------------------------------------------------------------
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_1200\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_1200, "ldo_dvt"); read_adc_value(ADC_ONLY);            
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_1300\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_1300, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);                   
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_1500\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_1500, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_1800\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);                        
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_2000\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_2000, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_2800\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_2800, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);  
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_3000\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_3000, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_3300\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_3300, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);            
        //---------------------------------------------------------------------------
    }
    else
    {
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] MT6331>=E2\n");
        
        //---------------------------------------------------------------------------
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_1200\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_1200, "ldo_dvt"); read_adc_value(ADC_ONLY);            
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_1600\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_1600, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);                   
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_1700\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_1700, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_1800\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);                        
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_1900\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_1900, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_2000\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_2000, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);  
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_2100\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_2100, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP4] VOL_2200\n");
                hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_2200, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);            
        //---------------------------------------------------------------------------
    }
}

void exec_6331_ldo_vsim1_vosel_test(void)
{
    int i=0;

    mt6331_upmu_set_rg_vsim1_on_ctrl(0);

    if(get_pmic_mt6331_cid()==PMIC6331_E1_CID_CODE)
    {
        printk("hwPowerOn(MT6331_POWER_LDO_VSIM1] MT6331==E1\n");        
        
        //---------------------------------------------------------------------------
        printk("hwPowerOn(MT6331_POWER_LDO_VSIM1] VOL_1800\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM1, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM1]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM1, "ldo_dvt"); read_adc_value(ADC_ONLY);

        printk("hwPowerOn(MT6331_POWER_LDO_VSIM1] VOL_3000\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM1, VOL_3000, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM1]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM1, "ldo_dvt"); read_adc_value(ADC_ONLY);
        //---------------------------------------------------------------------------        
    }
    else
    {
        printk("hwPowerOn(MT6331_POWER_LDO_VSIM1] MT6331>=E2\n");
        
        //---------------------------------------------------------------------------
        printk("hwPowerOn(MT6331_POWER_LDO_VSIM1] VOL_1700\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM1, VOL_1700, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM1]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM1, "ldo_dvt"); read_adc_value(ADC_ONLY);

        printk("hwPowerOn(MT6331_POWER_LDO_VSIM1] VOL_1800\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM1, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM1]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM1, "ldo_dvt"); read_adc_value(ADC_ONLY);

        printk("hwPowerOn(MT6331_POWER_LDO_VSIM1] VOL_1860\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM1, VOL_1860, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM1]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM1, "ldo_dvt"); read_adc_value(ADC_ONLY);        

        printk("hwPowerOn(MT6331_POWER_LDO_VSIM1] VOL_2760\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM1, VOL_2760, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM1]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM1, "ldo_dvt"); read_adc_value(ADC_ONLY);

        printk("hwPowerOn(MT6331_POWER_LDO_VSIM1] VOL_3000\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM1, VOL_3000, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM1]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM1, "ldo_dvt"); read_adc_value(ADC_ONLY);

        printk("hwPowerOn(MT6331_POWER_LDO_VSIM1] VOL_3100\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM1, VOL_3100, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM1]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM1, "ldo_dvt"); read_adc_value(ADC_ONLY);
        //---------------------------------------------------------------------------        
    }

    mt6331_upmu_set_rg_vsim1_en(1);
    for(i=0;i<=7;i++)
    {
        printk("mt6331_upmu_set_rg_vsim1_vosel=%d\n", i);
        mt6331_upmu_set_rg_vsim1_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vsim2_vosel_test(void)
{
    int i=0;
    
    mt6331_upmu_set_rg_vsim2_on_ctrl(0);

    if(get_pmic_mt6331_cid()==PMIC6331_E1_CID_CODE)
    {
        printk("hwPowerOn(MT6331_POWER_LDO_VSIM2] MT6331==E1\n");
        
        //---------------------------------------------------------------------------
        printk("hwPowerOn(MT6331_POWER_LDO_VSIM2] VOL_1800\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM2, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM2]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM2, "ldo_dvt"); read_adc_value(ADC_ONLY);

        printk("hwPowerOn(MT6331_POWER_LDO_VSIM2] VOL_3000\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM2, VOL_3000, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM2]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM2, "ldo_dvt"); read_adc_value(ADC_ONLY);
        //---------------------------------------------------------------------------        
    }
    else
    {
        printk("hwPowerOn(MT6331_POWER_LDO_VSIM2] MT6331>=E2\n");

        //---------------------------------------------------------------------------
        printk("hwPowerOn(MT6331_POWER_LDO_VSIM2] VOL_1700\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM2, VOL_1700, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM2]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM2, "ldo_dvt"); read_adc_value(ADC_ONLY);

        printk("hwPowerOn(MT6331_POWER_LDO_VSIM2] VOL_1800\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM2, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM2]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM2, "ldo_dvt"); read_adc_value(ADC_ONLY);
                
        printk("hwPowerOn(MT6331_POWER_LDO_VSIM2] VOL_1860\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM2, VOL_1860, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM2]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM2, "ldo_dvt"); read_adc_value(ADC_ONLY);                
                
        printk("hwPowerOn(MT6331_POWER_LDO_VSIM2] VOL_2760\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM2, VOL_2760, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM2]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM2, "ldo_dvt"); read_adc_value(ADC_ONLY);

        printk("hwPowerOn(MT6331_POWER_LDO_VSIM2] VOL_3000\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM2, VOL_3000, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM2]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM2, "ldo_dvt"); read_adc_value(ADC_ONLY);
                
        printk("hwPowerOn(MT6331_POWER_LDO_VSIM2] VOL_3100\n");
                hwPowerOn(MT6331_POWER_LDO_VSIM2, VOL_3100, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VSIM2]\n");
                hwPowerDown(MT6331_POWER_LDO_VSIM2, "ldo_dvt"); read_adc_value(ADC_ONLY);                                
        //---------------------------------------------------------------------------    
    }
    
    mt6331_upmu_set_rg_vsim2_en(1);
    for(i=0;i<=7;i++)
    {
        printk("mt6331_upmu_set_rg_vsim2_vosel=%d\n", i);
        mt6331_upmu_set_rg_vsim2_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vfbb_vosel_test(void)
{
    int i=0;
    
    mt6331_upmu_set_rg_vfbb_en(1);
    for(i=0;i<=7;i++)
    {
        printk("mt6331_upmu_set_rg_vfbb_vosel=%d\n", i);
        mt6331_upmu_set_rg_vfbb_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vibr_vosel_test(void)
{   
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VIBR] VOL_1200\n");
            hwPowerOn(MT6331_POWER_LDO_VIBR, VOL_1200, "ldo_dvt"); read_adc_value(ADC_ONLY);            
    printk("hwPowerDown(MT6331_POWER_LDO_VIBR]\n");
            hwPowerDown(MT6331_POWER_LDO_VIBR, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VIBR] VOL_1300\n");
            hwPowerOn(MT6331_POWER_LDO_VIBR, VOL_1300, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VIBR]\n");
            hwPowerDown(MT6331_POWER_LDO_VIBR, "ldo_dvt"); read_adc_value(ADC_ONLY);                   
            
    printk("hwPowerOn(MT6331_POWER_LDO_VIBR] VOL_1500\n");
            hwPowerOn(MT6331_POWER_LDO_VIBR, VOL_1500, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VIBR]\n");
            hwPowerDown(MT6331_POWER_LDO_VIBR, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VIBR] VOL_1800\n");
            hwPowerOn(MT6331_POWER_LDO_VIBR, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VIBR]\n");
            hwPowerDown(MT6331_POWER_LDO_VIBR, "ldo_dvt"); read_adc_value(ADC_ONLY);                        
            
    printk("hwPowerOn(MT6331_POWER_LDO_VIBR] VOL_2000\n");
            hwPowerOn(MT6331_POWER_LDO_VIBR, VOL_2000, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VIBR]\n");
            hwPowerDown(MT6331_POWER_LDO_VIBR, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VIBR] VOL_2800\n");
            hwPowerOn(MT6331_POWER_LDO_VIBR, VOL_2800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VIBR]\n");
            hwPowerDown(MT6331_POWER_LDO_VIBR, "ldo_dvt"); read_adc_value(ADC_ONLY);  
            
    printk("hwPowerOn(MT6331_POWER_LDO_VIBR] VOL_3000\n");
            hwPowerOn(MT6331_POWER_LDO_VIBR, VOL_3000, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VIBR]\n");
            hwPowerDown(MT6331_POWER_LDO_VIBR, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VIBR] VOL_3300\n");
            hwPowerOn(MT6331_POWER_LDO_VIBR, VOL_3300, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VIBR]\n");
            hwPowerDown(MT6331_POWER_LDO_VIBR, "ldo_dvt"); read_adc_value(ADC_ONLY);            
    //---------------------------------------------------------------------------        
}

void exec_6331_ldo_vrtc_vosel_test(void)
{   
    printk("hwPowerOn(MT6331_POWER_LDO_VRTC] VOL_DEFAULT\n");
            hwPowerOn(MT6331_POWER_LDO_VRTC, VOL_DEFAULT, "ldo_dvt"); read_adc_value(LDO_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VRTC]\n");
            hwPowerDown(MT6331_POWER_LDO_VRTC, "ldo_dvt"); read_adc_value(LDO_ONLY);     
}

void exec_6331_vdig18_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6331_PMIC_RG_VDIG18_VOSEL_MASK;k++) {
        mt6331_upmu_set_rg_vdig18_vosel(k); printk("[mt6331_upmu_set_rg_vdig18_vosel] k=%d, ",k);
        read_adc_value(LDO_ONLY);
    }
    for(k=0;k<=MT6331_PMIC_RG_VDIG18_SLEEP_VOSEL_MASK;k++) {
        mt6331_upmu_set_rg_vdig18_sleep_vosel(k); printk("[mt6331_upmu_set_rg_vdig18_sleep_vosel] k=%d, ",k);
        read_adc_value(LDO_ONLY);
    }
}

void exec_6331_vdig18_vosel_test_sub(void)
{
    printk("[exec_6331_vdig18_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_6331_vdig18_vosel_test_sub_vosel();

    printk("[exec_6331_vdig18_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_6331_vdig18_vosel_test_sub_vosel();

    printk("[exec_6331_vdig18_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_6331_vdig18_vosel_test_sub_vosel();

    printk("[exec_6331_vdig18_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_6331_vdig18_vosel_test_sub_vosel();
}

void exec_6331_ldo_vdig18_vosel_test(void)
{
    int i=0, k=0;

    mt6331_upmu_set_rg_vdig18_vosel_ctrl(0);
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VDIG18] VOL_1200\n");
            hwPowerOn(MT6331_POWER_LDO_VDIG18, VOL_1200, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VDIG18]\n");
            hwPowerDown(MT6331_POWER_LDO_VDIG18, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerOn(MT6331_POWER_LDO_VDIG18] VOL_1300\n");
            hwPowerOn(MT6331_POWER_LDO_VDIG18, VOL_1300, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VDIG18]\n");
            hwPowerDown(MT6331_POWER_LDO_VDIG18, "ldo_dvt"); read_adc_value(ADC_ONLY);        
    printk("hwPowerOn(MT6331_POWER_LDO_VDIG18] VOL_1400\n");
            hwPowerOn(MT6331_POWER_LDO_VDIG18, VOL_1400, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VDIG18]\n");
            hwPowerDown(MT6331_POWER_LDO_VDIG18, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerOn(MT6331_POWER_LDO_VDIG18] VOL_1500\n");
            hwPowerOn(MT6331_POWER_LDO_VDIG18, VOL_1500, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VDIG18]\n");
            hwPowerDown(MT6331_POWER_LDO_VDIG18, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerOn(MT6331_POWER_LDO_VDIG18] VOL_1600\n");
            hwPowerOn(MT6331_POWER_LDO_VDIG18, VOL_1600, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VDIG18]\n");
            hwPowerDown(MT6331_POWER_LDO_VDIG18, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerOn(MT6331_POWER_LDO_VDIG18] VOL_1700\n");
            hwPowerOn(MT6331_POWER_LDO_VDIG18, VOL_1700, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VDIG18]\n");
            hwPowerDown(MT6331_POWER_LDO_VDIG18, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerOn(MT6331_POWER_LDO_VDIG18] VOL_1800\n");
            hwPowerOn(MT6331_POWER_LDO_VDIG18, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VDIG18]\n");
            hwPowerDown(MT6331_POWER_LDO_VDIG18, "ldo_dvt"); read_adc_value(ADC_ONLY);
    //---------------------------------------------------------------------------        

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vdig18_vosel_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vdig18_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6331_PMIC_RG_VDIG18_VOSEL_MASK;k++) {
                    mt6331_upmu_set_rg_vdig18_vosel(k); printk("[mt6331_upmu_set_rg_vdig18_vosel] k=%d, ",k);
                    read_adc_value(ADC_ONLY);
                }
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vdig18_srclken_sel(0)]\n");
                mt6331_upmu_set_rg_vdig18_srclken_sel(0);
                exec_6331_vdig18_vosel_test_sub(); 

                printk("[mt6331_upmu_set_rg_vdig18_srclken_sel(1)]\n");
                mt6331_upmu_set_rg_vdig18_srclken_sel(1);                
                exec_6331_vdig18_vosel_test_sub();

                printk("[mt6331_upmu_set_rg_vdig18_srclken_sel(2)]\n");
                mt6331_upmu_set_rg_vdig18_srclken_sel(2);
                exec_6331_vdig18_vosel_test_sub();

                printk("[mt6331_upmu_set_rg_vdig18_srclken_sel(3)]\n");
                mt6331_upmu_set_rg_vdig18_srclken_sel(3);
                exec_6331_vdig18_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_6331_ldo_vmipi_vosel_test(void)
{   
    int i=0;
    
    mt6331_upmu_set_rg_vmipi_on_ctrl(0);
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VMIPI] VOL_1200\n");
            hwPowerOn(MT6331_POWER_LDO_VMIPI, VOL_1200, "ldo_dvt"); read_adc_value(ADC_ONLY);            
    printk("hwPowerDown(MT6331_POWER_LDO_VMIPI]\n");
            hwPowerDown(MT6331_POWER_LDO_VMIPI, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VMIPI] VOL_1300\n");
            hwPowerOn(MT6331_POWER_LDO_VMIPI, VOL_1300, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VMIPI]\n");
            hwPowerDown(MT6331_POWER_LDO_VMIPI, "ldo_dvt"); read_adc_value(ADC_ONLY);                   
            
    printk("hwPowerOn(MT6331_POWER_LDO_VMIPI] VOL_1500\n");
            hwPowerOn(MT6331_POWER_LDO_VMIPI, VOL_1500, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VMIPI]\n");
            hwPowerDown(MT6331_POWER_LDO_VMIPI, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VMIPI] VOL_1800\n");
            hwPowerOn(MT6331_POWER_LDO_VMIPI, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VMIPI]\n");
            hwPowerDown(MT6331_POWER_LDO_VMIPI, "ldo_dvt"); read_adc_value(ADC_ONLY);                        
            
    printk("hwPowerOn(MT6331_POWER_LDO_VMIPI] VOL_2000\n");
            hwPowerOn(MT6331_POWER_LDO_VMIPI, VOL_2000, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VMIPI]\n");
            hwPowerDown(MT6331_POWER_LDO_VMIPI, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VMIPI] VOL_2800\n");
            hwPowerOn(MT6331_POWER_LDO_VMIPI, VOL_2800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VMIPI]\n");
            hwPowerDown(MT6331_POWER_LDO_VMIPI, "ldo_dvt"); read_adc_value(ADC_ONLY);  
            
    printk("hwPowerOn(MT6331_POWER_LDO_VMIPI] VOL_3000\n");
            hwPowerOn(MT6331_POWER_LDO_VMIPI, VOL_3000, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VMIPI]\n");
            hwPowerDown(MT6331_POWER_LDO_VMIPI, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VMIPI] VOL_3300\n");
            hwPowerOn(MT6331_POWER_LDO_VMIPI, VOL_3300, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VMIPI]\n");
            hwPowerDown(MT6331_POWER_LDO_VMIPI, "ldo_dvt"); read_adc_value(ADC_ONLY);            
    //---------------------------------------------------------------------------        
    mt6331_upmu_set_rg_vmipi_en(1);
    for(i=0;i<=15;i++)
    {
        printk("mt6331_upmu_set_rg_vmipi_vosel=%d\n", i);
        mt6331_upmu_set_rg_vmipi_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vcamd_vosel_test(void)
{   
    int i=0;
    
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VCAMD] VOL_0900\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMD, VOL_0900, "ldo_dvt"); read_adc_value(ADC_ONLY);            
    printk("hwPowerDown(MT6331_POWER_LDO_VCAMD]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMD, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAMD] VOL_1000\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMD, VOL_1000, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAMD]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMD, "ldo_dvt"); read_adc_value(ADC_ONLY);                   
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAMD] VOL_1100\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMD, VOL_1100, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAMD]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMD, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAMD] VOL_1220\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMD, VOL_1220, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAMD]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMD, "ldo_dvt"); read_adc_value(ADC_ONLY);                        
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAMD] VOL_1300\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMD, VOL_1300, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAMD]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMD, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAMD] VOL_1500\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMD, VOL_1500, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAMD]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMD, "ldo_dvt"); read_adc_value(ADC_ONLY);             
    //---------------------------------------------------------------------------        
    mt6331_upmu_set_rg_vcamd_en(1);
    for(i=0;i<=7;i++)
    {
        printk("mt6331_upmu_set_rg_vcamd_vosel=%d\n", i);
        mt6331_upmu_set_rg_vcamd_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vusb10_vosel_test(void)
{   
    int i=0;
    
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VUSB10] VOL_1000\n");
            hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_1000, "ldo_dvt"); read_adc_value(ADC_ONLY);            
    printk("hwPowerDown(MT6331_POWER_LDO_VUSB10]\n");
            hwPowerDown(MT6331_POWER_LDO_VUSB10, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VUSB10] VOL_1050\n");
            hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_1050, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VUSB10]\n");
            hwPowerDown(MT6331_POWER_LDO_VUSB10, "ldo_dvt"); read_adc_value(ADC_ONLY);                   
            
    printk("hwPowerOn(MT6331_POWER_LDO_VUSB10] VOL_1100\n");
            hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_1100, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VUSB10]\n");
            hwPowerDown(MT6331_POWER_LDO_VUSB10, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VUSB10] VOL_1150\n");
            hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_1150, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VUSB10]\n");
            hwPowerDown(MT6331_POWER_LDO_VUSB10, "ldo_dvt"); read_adc_value(ADC_ONLY);                        
            
    printk("hwPowerOn(MT6331_POWER_LDO_VUSB10] VOL_1200\n");
            hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_1200, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VUSB10]\n");
            hwPowerDown(MT6331_POWER_LDO_VUSB10, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VUSB10] VOL_1250\n");
            hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_1250, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VUSB10]\n");
            hwPowerDown(MT6331_POWER_LDO_VUSB10, "ldo_dvt"); read_adc_value(ADC_ONLY);  
            
    printk("hwPowerOn(MT6331_POWER_LDO_VUSB10] VOL_1300\n");
            hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_1300, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VUSB10]\n");
            hwPowerDown(MT6331_POWER_LDO_VUSB10, "ldo_dvt"); read_adc_value(ADC_ONLY);            
    //---------------------------------------------------------------------------        
    mt6331_upmu_set_rg_vusb10_en(1);
    for(i=0;i<=15;i++)
    {
        printk("mt6331_upmu_set_rg_vusb10_vosel=%d\n", i);
        mt6331_upmu_set_rg_vusb10_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vcam_io_vosel_test(void)
{   
    int i=0;
    
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_IO] VOL_1200\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_IO, VOL_1200, "ldo_dvt"); read_adc_value(ADC_ONLY);            
    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_IO]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_IO, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_IO] VOL_1300\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_IO, VOL_1300, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_IO]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_IO, "ldo_dvt"); read_adc_value(ADC_ONLY);                   
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_IO] VOL_1500\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_IO, VOL_1500, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_IO]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_IO, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_IO] VOL_1800\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_IO, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_IO]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_IO, "ldo_dvt"); read_adc_value(ADC_ONLY);                        
    //---------------------------------------------------------------------------        
    mt6331_upmu_set_rg_vcam_io_en(1);
    for(i=0;i<=15;i++)
    {
        printk("mt6331_upmu_set_rg_vcam_io_vosel=%d\n", i);
        mt6331_upmu_set_rg_vcam_io_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vsram_dvfs1_vosel_test(void)
{
    int i=0;

    mt6331_upmu_set_rg_vsram_dvfs1_on_ctrl(0);
    mt6331_upmu_set_rg_vsram_dvfs1_en(1);
    for(i=0;i<=0x7F;i++)
    {
        printk("mt6331_upmu_set_rg_vsram_dvfs1_vosel=%d\n", i);
        mt6331_upmu_set_rg_vsram_dvfs1_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vgp2_vosel_test(void)
{   
    int i=0;

    if(get_pmic_mt6331_cid()==PMIC6331_E1_CID_CODE)
    {
        printk("hwPowerOn(MT6331_POWER_LDO_VGP2] MT6331==E1\n");
        
        //---------------------------------------------------------------------------
        printk("hwPowerOn(MT6331_POWER_LDO_VGP2] VOL_1200\n");
                hwPowerOn(MT6331_POWER_LDO_VGP2, VOL_1200, "ldo_dvt"); read_adc_value(ADC_ONLY);            
        printk("hwPowerDown(MT6331_POWER_LDO_VGP2]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP2, "ldo_dvt"); read_adc_value(ADC_ONLY);
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP2] VOL_1300\n");
                hwPowerOn(MT6331_POWER_LDO_VGP2, VOL_1300, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP2]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP2, "ldo_dvt"); read_adc_value(ADC_ONLY);                   
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP2] VOL_1500\n");
                hwPowerOn(MT6331_POWER_LDO_VGP2, VOL_1500, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP2]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP2, "ldo_dvt"); read_adc_value(ADC_ONLY);
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP2] VOL_1800\n");
                hwPowerOn(MT6331_POWER_LDO_VGP2, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP2]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP2, "ldo_dvt"); read_adc_value(ADC_ONLY);                        
        //---------------------------------------------------------------------------        
    }
    else
    {
        printk("hwPowerOn(MT6331_POWER_LDO_VGP2] MT6331>=E2\n");

        //---------------------------------------------------------------------------
        printk("hwPowerOn(MT6331_POWER_LDO_VGP2] VOL_1200\n");
                hwPowerOn(MT6331_POWER_LDO_VGP2, VOL_1200, "ldo_dvt"); read_adc_value(ADC_ONLY);            
        printk("hwPowerDown(MT6331_POWER_LDO_VGP2]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP2, "ldo_dvt"); read_adc_value(ADC_ONLY);
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP2] VOL_1360\n");
                hwPowerOn(MT6331_POWER_LDO_VGP2, VOL_1360, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP2]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP2, "ldo_dvt"); read_adc_value(ADC_ONLY);                   
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP2] VOL_1500\n");
                hwPowerOn(MT6331_POWER_LDO_VGP2, VOL_1500, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP2]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP2, "ldo_dvt"); read_adc_value(ADC_ONLY);
                
        printk("hwPowerOn(MT6331_POWER_LDO_VGP2] VOL_1100\n");
                hwPowerOn(MT6331_POWER_LDO_VGP2, VOL_1100, "ldo_dvt"); read_adc_value(ADC_ONLY);
        printk("hwPowerDown(MT6331_POWER_LDO_VGP2]\n");
                hwPowerDown(MT6331_POWER_LDO_VGP2, "ldo_dvt"); read_adc_value(ADC_ONLY);                        
        //---------------------------------------------------------------------------
    }
    
    mt6331_upmu_set_rg_vgp2_en(1);
    for(i=0;i<=15;i++)
    {
        printk("mt6331_upmu_set_rg_vgp2_vosel=%d\n", i);
        mt6331_upmu_set_rg_vgp2_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vgp3_vosel_test(void)
{   
    int i=0;
    
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6331_POWER_LDO_VGP3] VOL_1200\n");
            hwPowerOn(MT6331_POWER_LDO_VGP3, VOL_1200, "ldo_dvt"); read_adc_value(ADC_ONLY);            
    printk("hwPowerDown(MT6331_POWER_LDO_VGP3]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP3, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VGP3] VOL_1300\n");
            hwPowerOn(MT6331_POWER_LDO_VGP3, VOL_1300, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VGP3]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP3, "ldo_dvt"); read_adc_value(ADC_ONLY);                   
            
    printk("hwPowerOn(MT6331_POWER_LDO_VGP3] VOL_1500\n");
            hwPowerOn(MT6331_POWER_LDO_VGP3, VOL_1500, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VGP3]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP3, "ldo_dvt"); read_adc_value(ADC_ONLY);
            
    printk("hwPowerOn(MT6331_POWER_LDO_VGP3] VOL_1800\n");
            hwPowerOn(MT6331_POWER_LDO_VGP3, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6331_POWER_LDO_VGP3]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP3, "ldo_dvt"); read_adc_value(ADC_ONLY);                        
    //---------------------------------------------------------------------------        
    mt6331_upmu_set_rg_vgp3_en(1);
    for(i=0;i<=15;i++)
    {
        printk("mt6331_upmu_set_rg_vgp3_vosel=%d\n", i);
        mt6331_upmu_set_rg_vgp3_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6331_ldo_vbiasn_vosel_test(void)
{
    int i=0;
    
    mt6331_upmu_set_rg_vbiasn_en(1);
    for(i=0;i<=31;i++)
    {
        printk("mt6331_upmu_set_rg_vbiasn_vosel=%d\n", i);
        mt6331_upmu_set_rg_vbiasn_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6332_ldo_vauxb32_vosel_test(void)
{
    int i=0;
    
    mt6332_upmu_set_rg_vauxb32_on_ctrl(0);
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6332_POWER_LDO_VAUXB32] VOL_2800\n");
            hwPowerOn(MT6332_POWER_LDO_VAUXB32, VOL_2800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6332_POWER_LDO_VAUXB32]\n");
            hwPowerDown(MT6332_POWER_LDO_VAUXB32, "ldo_dvt"); read_adc_value(ADC_ONLY);

    printk("hwPowerOn(MT6332_POWER_LDO_VAUXB32] VOL_3000\n");
            hwPowerOn(MT6332_POWER_LDO_VAUXB32, VOL_3000, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6332_POWER_LDO_VAUXB32]\n");
            hwPowerDown(MT6332_POWER_LDO_VAUXB32, "ldo_dvt"); read_adc_value(ADC_ONLY);

    printk("hwPowerOn(MT6332_POWER_LDO_VAUXB32] VOL_3200\n");
            hwPowerOn(MT6332_POWER_LDO_VAUXB32, VOL_3200, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6332_POWER_LDO_VAUXB32]\n");
            hwPowerDown(MT6332_POWER_LDO_VAUXB32, "ldo_dvt"); read_adc_value(ADC_ONLY);        
    //---------------------------------------------------------------------------
    mt6332_upmu_set_rg_vauxb32_en(1);
    for(i=0;i<=3;i++)
    {
        printk("mt6332_upmu_set_rg_vauxb32_vosel=%d\n", i);
        mt6332_upmu_set_rg_vauxb32_vosel(i); 
        read_adc_value(ADC_ONLY);
    }
}

void exec_6332_ldo_vbif28_vosel_test(void)
{
    mt6332_upmu_set_rg_vbif28_on_ctrl(0);
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6332_POWER_LDO_VBIF28] VOL_DEFAULT\n");
            hwPowerOn(MT6332_POWER_LDO_VBIF28, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6332_POWER_LDO_VBIF28]\n");
            hwPowerDown(MT6332_POWER_LDO_VBIF28, "ldo_dvt"); read_adc_value(ADC_ONLY);
    //---------------------------------------------------------------------------
}

void exec_6332_ldo_vusb33_vosel_test(void)
{
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6332_POWER_LDO_VUSB33] VOL_DEFAULT\n");
            hwPowerOn(MT6332_POWER_LDO_VUSB33, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6332_POWER_LDO_VUSB33]\n");
            hwPowerDown(MT6332_POWER_LDO_VUSB33, "ldo_dvt"); read_adc_value(ADC_ONLY);
    //---------------------------------------------------------------------------
}

void exec_6332_vdig18_vosel_test_sub_vosel(void)
{
    int k=0;
        
    for(k=0;k<=MT6332_PMIC_RG_VDIG18_VOSEL_MASK;k++) {
        mt6332_upmu_set_rg_vdig18_vosel(k); printk("[mt6332_upmu_set_rg_vdig18_vosel] k=%d, ",k);
        read_adc_value(LDO_ONLY);
    }
    for(k=0;k<=MT6332_PMIC_RG_VDIG18_SLEEP_VOSEL_MASK;k++) {
        mt6332_upmu_set_rg_vdig18_sleep_vosel(k); printk("[mt6332_upmu_set_rg_vdig18_sleep_vosel] k=%d, ",k);
        read_adc_value(LDO_ONLY);
    }
}

void exec_6332_vdig18_vosel_test_sub(void)
{
    printk("[exec_6332_vdig18_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(0);\n");
                              set_srclken_1_val(0); set_srclken_2_val(0); exec_6332_vdig18_vosel_test_sub_vosel();

    printk("[exec_6332_vdig18_vosel_test_sub] set_srclken_1_val(0); set_srclken_2_val(1);\n");
                              set_srclken_1_val(0); set_srclken_2_val(1); exec_6332_vdig18_vosel_test_sub_vosel();

    printk("[exec_6332_vdig18_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(0);\n");
                              set_srclken_1_val(1); set_srclken_2_val(0); exec_6332_vdig18_vosel_test_sub_vosel();

    printk("[exec_6332_vdig18_vosel_test_sub] set_srclken_1_val(1); set_srclken_2_val(1);\n");
                              set_srclken_1_val(1); set_srclken_2_val(1); exec_6332_vdig18_vosel_test_sub_vosel();
}

void exec_6332_ldo_vdig18_vosel_test(void)
{
    int i=0, k=0;

    mt6332_upmu_set_rg_vdig18_vosel_ctrl(0);
    //---------------------------------------------------------------------------
    printk("hwPowerOn(MT6332_POWER_LDO_VDIG18] VOL_1200\n");
            hwPowerOn(MT6332_POWER_LDO_VDIG18, VOL_1200, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6332_POWER_LDO_VDIG18]\n");
            hwPowerDown(MT6332_POWER_LDO_VDIG18, "ldo_dvt"); read_adc_value(ADC_ONLY);           
    printk("hwPowerOn(MT6332_POWER_LDO_VDIG18] VOL_1300\n");
            hwPowerOn(MT6332_POWER_LDO_VDIG18, VOL_1300, "ldo_dvt"); read_adc_value(ADC_ONLY);        
    printk("hwPowerDown(MT6332_POWER_LDO_VDIG18]\n");
            hwPowerDown(MT6332_POWER_LDO_VDIG18, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerOn(MT6332_POWER_LDO_VDIG18] VOL_1400\n");
            hwPowerOn(MT6332_POWER_LDO_VDIG18, VOL_1400, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6332_POWER_LDO_VDIG18]\n");
            hwPowerDown(MT6332_POWER_LDO_VDIG18, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerOn(MT6332_POWER_LDO_VDIG18] VOL_1500\n");
            hwPowerOn(MT6332_POWER_LDO_VDIG18, VOL_1500, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6332_POWER_LDO_VDIG18]\n");
            hwPowerDown(MT6332_POWER_LDO_VDIG18, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerOn(MT6332_POWER_LDO_VDIG18] VOL_1600\n");
            hwPowerOn(MT6332_POWER_LDO_VDIG18, VOL_1600, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6332_POWER_LDO_VDIG18]\n");
            hwPowerDown(MT6332_POWER_LDO_VDIG18, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerOn(MT6332_POWER_LDO_VDIG18] VOL_1700\n");
            hwPowerOn(MT6332_POWER_LDO_VDIG18, VOL_1700, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6332_POWER_LDO_VDIG18]\n");
            hwPowerDown(MT6332_POWER_LDO_VDIG18, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerOn(MT6332_POWER_LDO_VDIG18] VOL_1800\n");
            hwPowerOn(MT6332_POWER_LDO_VDIG18, VOL_1800, "ldo_dvt"); read_adc_value(ADC_ONLY);
    printk("hwPowerDown(MT6332_POWER_LDO_VDIG18]\n");
            hwPowerDown(MT6332_POWER_LDO_VDIG18, "ldo_dvt"); read_adc_value(ADC_ONLY);
    //---------------------------------------------------------------------------        

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_rg_vdig18_vosel_ctrl] %d\n", i);
        mt6332_upmu_set_rg_vdig18_vosel_ctrl(i);

        switch(i){
            case 0:
                for(k=0;k<=MT6332_PMIC_RG_VDIG18_VOSEL_MASK;k++) {
                    mt6332_upmu_set_rg_vdig18_vosel(k); printk("[mt6332_upmu_set_rg_vdig18_vosel] k=%d, ",k);
                    read_adc_value(ADC_ONLY);
                }
                break;

            case 1:
                printk("[mt6332_upmu_set_rg_vdig18_srclken_sel(0)]\n");
                mt6332_upmu_set_rg_vdig18_srclken_sel(0);
                exec_6332_vdig18_vosel_test_sub(); 

                printk("[mt6332_upmu_set_rg_vdig18_srclken_sel(1)]\n");
                mt6332_upmu_set_rg_vdig18_srclken_sel(1);                
                exec_6332_vdig18_vosel_test_sub();

                printk("[mt6332_upmu_set_rg_vdig18_srclken_sel(2)]\n");
                mt6332_upmu_set_rg_vdig18_srclken_sel(2);
                exec_6332_vdig18_vosel_test_sub();

                printk("[mt6332_upmu_set_rg_vdig18_srclken_sel(3)]\n");
                mt6332_upmu_set_rg_vdig18_srclken_sel(3);
                exec_6332_vdig18_vosel_test_sub();
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }
}

void exec_6332_ldo_vsram_dvfs2_vosel_test(void)
{
    int i=0;

    mt6332_upmu_set_rg_vsram_dvfs2_on_ctrl(0);
    mt6332_upmu_set_rg_vsram_dvfs2_en(1);
    
    for(i=0;i<=0x7F;i++)
    //for(i=1;i<=0x7F;i++)
    //for(i=0x32;i<=0x7F;i++)
    {
        printk("mt6332_upmu_set_rg_vsram_dvfs2_vosel=%d\n", i);
        mt6332_upmu_set_rg_vsram_dvfs2_vosel(i); 
        mdelay(500);
        read_adc_value(ADC_ONLY);
    }
}
#endif

#ifdef LDO_CAL_DVT_ENABLE
void exec_6331_ldo_vtcxo1_cal_test(void)
{
    int i=0;    
    mt6331_upmu_set_rg_vtcxo1_on_ctrl(0);

    printk("hwPowerOn(MT6331_POWER_LDO_VTCXO1]\n");
            hwPowerOn(MT6331_POWER_LDO_VTCXO1, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VTCXO1_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vtcxo1_cal=%d, ", i);
        mt6331_upmu_set_rg_vtcxo1_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VTCXO1]\n");
            hwPowerDown(MT6331_POWER_LDO_VTCXO1, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vtcxo2_cal_test(void)
{
    int i=0;    
    mt6331_upmu_set_rg_vtcxo2_on_ctrl(0);

    printk("hwPowerOn(MT6331_POWER_LDO_VTCXO2]\n");
            hwPowerOn(MT6331_POWER_LDO_VTCXO2, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VTCXO2_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vtcxo2_cal=%d, ", i);
        mt6331_upmu_set_rg_vtcxo2_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VTCXO2]\n");
            hwPowerDown(MT6331_POWER_LDO_VTCXO2, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vauxa32_cal_test(void)
{
    int i=0;    
    mt6331_upmu_set_rg_vauxa32_on_ctrl(0);

    printk("hwPowerOn(MT6331_POWER_LDO_VAUXA32]\n");
            hwPowerOn(MT6331_POWER_LDO_VAUXA32, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VAUXA32_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vauxa32_cal=%d, ", i);
        mt6331_upmu_set_rg_vauxa32_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VAUXA32]\n");
            hwPowerDown(MT6331_POWER_LDO_VAUXA32, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vaud32_cal_test(void)
{
    int i=0;    
    mt6331_upmu_set_rg_vaud32_on_ctrl(0);

    printk("hwPowerOn(MT6331_POWER_LDO_VAUD32]\n");
            hwPowerOn(MT6331_POWER_LDO_VAUD32, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VAUD32_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vaud32_cal=%d, ", i);
        mt6331_upmu_set_rg_vaud32_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VAUD32]\n");
            hwPowerDown(MT6331_POWER_LDO_VAUD32, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vcama_cal_test(void)
{
    int i=0;    


    printk("hwPowerOn(MT6331_POWER_LDO_VCAMA]\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMA, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VCAMA_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vcama_cal=%d, ", i);
        mt6331_upmu_set_rg_vcama_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VCAMA]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMA, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vio28_cal_test(void)
{
    int i=0;    


    printk("hwPowerOn(MT6331_POWER_LDO_VIO28]\n");
            hwPowerOn(MT6331_POWER_LDO_VIO28, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VIO28_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vio28_cal=%d, ", i);
        mt6331_upmu_set_rg_vio28_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VIO28]\n");
            hwPowerDown(MT6331_POWER_LDO_VIO28, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vcam_af_cal_test(void)
{
    int i=0;    


    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_AF]\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_AF, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VCAM_AF_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vcam_af_cal=%d, ", i);
        mt6331_upmu_set_rg_vcam_af_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_AF]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_AF, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vmc_cal_test(void)
{
    int i=0;    
    mt6331_upmu_set_rg_vmc_on_ctrl(0);

    printk("hwPowerOn(MT6331_POWER_LDO_VMC]\n");
            hwPowerOn(MT6331_POWER_LDO_VMC, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VMC_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vmc_cal=%d, ", i);
        mt6331_upmu_set_rg_vmc_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VMC]\n");
            hwPowerDown(MT6331_POWER_LDO_VMC, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vmch_cal_test(void)
{
    int i=0;    
    mt6331_upmu_set_rg_vmch_on_ctrl(0);

    printk("hwPowerOn(MT6331_POWER_LDO_VMCH]\n");
            hwPowerOn(MT6331_POWER_LDO_VMCH, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VMCH_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vmch_cal=%d, ", i);
        mt6331_upmu_set_rg_vmch_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VMCH]\n");
            hwPowerDown(MT6331_POWER_LDO_VMCH, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vemc33_cal_test(void)
{
    int i=0;    
    mt6331_upmu_set_rg_vemc33_on_ctrl(0);

    printk("hwPowerOn(MT6331_POWER_LDO_VEMC33]\n");
            hwPowerOn(MT6331_POWER_LDO_VEMC33, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VEMC33_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vemc33_cal=%d, ", i);
        mt6331_upmu_set_rg_vemc33_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VEMC33]\n");
            hwPowerDown(MT6331_POWER_LDO_VEMC33, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vgp1_cal_test(void)
{
    int i=0;    


    printk("hwPowerOn(MT6331_POWER_LDO_VGP1]\n");
            hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VGP1_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vgp1_cal=%d, ", i);
        mt6331_upmu_set_rg_vgp1_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VGP1]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP1, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vgp4_cal_test(void)
{
    int i=0;    


    printk("hwPowerOn(MT6331_POWER_LDO_VGP4]\n");
            hwPowerOn(MT6331_POWER_LDO_VGP4, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VGP4_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vgp4_cal=%d, ", i);
        mt6331_upmu_set_rg_vgp4_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VGP4]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP4, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vsim1_cal_test(void)
{
    int i=0;    
    mt6331_upmu_set_rg_vsim1_on_ctrl(0);

    printk("hwPowerOn(MT6331_POWER_LDO_VSIM1]\n");
            hwPowerOn(MT6331_POWER_LDO_VSIM1, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VSIM1_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vsim1_cal=%d, ", i);
        mt6331_upmu_set_rg_vsim1_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VSIM1]\n");
            hwPowerDown(MT6331_POWER_LDO_VSIM1, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vsim2_cal_test(void)
{
    int i=0;    
    mt6331_upmu_set_rg_vsim2_on_ctrl(0);

    printk("hwPowerOn(MT6331_POWER_LDO_VSIM2]\n");
            hwPowerOn(MT6331_POWER_LDO_VSIM2, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VSIM2_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vsim2_cal=%d, ", i);
        mt6331_upmu_set_rg_vsim2_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VSIM2]\n");
            hwPowerDown(MT6331_POWER_LDO_VSIM2, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vibr_cal_test(void)
{
    int i=0;    


    printk("hwPowerOn(MT6331_POWER_LDO_VIBR]\n");
            hwPowerOn(MT6331_POWER_LDO_VIBR, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VIBR_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vibr_cal=%d, ", i);
        mt6331_upmu_set_rg_vibr_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VIBR]\n");
            hwPowerDown(MT6331_POWER_LDO_VIBR, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vcamd_cal_test(void)
{
    int i=0;    


    printk("hwPowerOn(MT6331_POWER_LDO_VCAMD]\n");
            hwPowerOn(MT6331_POWER_LDO_VCAMD, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VCAMD_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vcamd_cal=%d, ", i);
        mt6331_upmu_set_rg_vcamd_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VCAMD]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAMD, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vusb10_cal_test(void)
{
    int i=0;    


    printk("hwPowerOn(MT6331_POWER_LDO_VUSB10]\n");
            hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VUSB10_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vusb10_cal=%d, ", i);
        mt6331_upmu_set_rg_vusb10_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VUSB10]\n");
            hwPowerDown(MT6331_POWER_LDO_VUSB10, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vcam_io_cal_test(void)
{
    int i=0;    


    printk("hwPowerOn(MT6331_POWER_LDO_VCAM_IO]\n");
            hwPowerOn(MT6331_POWER_LDO_VCAM_IO, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VCAM_IO_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vcam_io_cal=%d, ", i);
        mt6331_upmu_set_rg_vcam_io_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VCAM_IO]\n");
            hwPowerDown(MT6331_POWER_LDO_VCAM_IO, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vgp2_cal_test(void)
{
    int i=0;    


    printk("hwPowerOn(MT6331_POWER_LDO_VGP2]\n");
            hwPowerOn(MT6331_POWER_LDO_VGP2, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VGP2_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vgp2_cal=%d, ", i);
        mt6331_upmu_set_rg_vgp2_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VGP2]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP2, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vgp3_cal_test(void)
{
    int i=0;    


    printk("hwPowerOn(MT6331_POWER_LDO_VGP3]\n");
            hwPowerOn(MT6331_POWER_LDO_VGP3, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VGP3_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vgp3_cal=%d, ", i);
        mt6331_upmu_set_rg_vgp3_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VGP3]\n");
            hwPowerDown(MT6331_POWER_LDO_VGP3, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vbiasn_cal_test(void)
{
    int i=0;    


    printk("hwPowerOn(MT6331_POWER_LDO_VBIASN]\n");
            hwPowerOn(MT6331_POWER_LDO_VBIASN, VOL_0500, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VBIASN_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vbiasn_cal=%d, ", i);
        mt6331_upmu_set_rg_vbiasn_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VBIASN]\n");
            hwPowerDown(MT6331_POWER_LDO_VBIASN, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vmipi_cal_test(void)
{
    int i=0;    
    mt6331_upmu_set_rg_vmipi_on_ctrl(0);

    printk("hwPowerOn(MT6331_POWER_LDO_VMIPI]\n");
            hwPowerOn(MT6331_POWER_LDO_VMIPI, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6331_PMIC_RG_VMIPI_CAL_MASK;i++)
    {
        printk("mt6331_upmu_set_rg_vmipi_cal=%d, ", i);
        mt6331_upmu_set_rg_vmipi_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6331_POWER_LDO_VMIPI]\n");
            hwPowerDown(MT6331_POWER_LDO_VMIPI, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6331_ldo_vsram_dvfs1_cal_test(void)
{
    printk("no exec_6331_ldo_vsram_dvfs1_cal_test\n");
}

void exec_6331_ldo_vfbb_cal_test(void)
{
    printk("no exec_6331_ldo_vfbb_cal_test\n");
}

void exec_6331_ldo_vrtc_cal_test(void)
{
    printk("no exec_6331_ldo_vrtc_cal_test\n");
}

void exec_6331_ldo_vdig18_cal_test(void)
{
    printk("no exec_6331_ldo_vdig18_cal_test\n");
}

void exec_6332_ldo_vauxb32_cal_test(void)
{
    int i=0;    
    mt6332_upmu_set_rg_vauxb32_on_ctrl(0);

    printk("hwPowerOn(MT6332_POWER_LDO_VAUXB32]\n");
            hwPowerOn(MT6332_POWER_LDO_VAUXB32, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6332_PMIC_RG_VAUXB32_CAL_MASK;i++)
    {
        printk("mt6332_upmu_set_rg_vauxb32_cal=%d, ", i);
        mt6332_upmu_set_rg_vauxb32_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6332_POWER_LDO_VAUXB32]\n");
            hwPowerDown(MT6332_POWER_LDO_VAUXB32, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6332_ldo_vusb33_cal_test(void)
{
    int i=0;    


    printk("hwPowerOn(MT6332_POWER_LDO_VUSB33]\n");
            hwPowerOn(MT6332_POWER_LDO_VUSB33, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6332_PMIC_RG_VUSB33_CAL_MASK;i++)
    {
        printk("mt6332_upmu_set_rg_vusb33_cal=%d, ", i);
        mt6332_upmu_set_rg_vusb33_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6332_POWER_LDO_VUSB33]\n");
            hwPowerDown(MT6332_POWER_LDO_VUSB33, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6332_ldo_vbif28_cal_test(void)
{
    int i=0;    
    mt6332_upmu_set_rg_vbif28_on_ctrl(0);

    printk("hwPowerOn(MT6332_POWER_LDO_VBIF28]\n");
            hwPowerOn(MT6332_POWER_LDO_VBIF28, VOL_DEFAULT, "ldo_dvt"); read_adc_value(ADC_ONLY);        
            
    for(i=0;i<=MT6332_PMIC_RG_VBIF28_CAL_MASK;i++)
    {
        printk("mt6332_upmu_set_rg_vbif28_cal=%d, ", i);
        mt6332_upmu_set_rg_vbif28_cal(i); 
        read_adc_value(ADC_ONLY);
    }
    
    printk("hwPowerDown(MT6332_POWER_LDO_VBIF28]\n");
            hwPowerDown(MT6332_POWER_LDO_VBIF28, "ldo_dvt"); read_adc_value(ADC_ONLY);
}

void exec_6332_ldo_vsram_dvfs2_cal_test(void)
{
    printk("no exec_6332_ldo_vsram_dvfs2_cal_test\n");
}

void exec_6332_ldo_vdig18_cal_test(void)
{
    printk("no exec_6332_ldo_vdig18_cal_test\n");
}
#endif

#ifdef LDO_MODE_DVT_ENABLE
void exec_6331_ldo_vtcxo1_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vtcxo1_mode = %d\n", mt6331_upmu_get_qi_vtcxo1_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vtcxo1_mode = %d\n", mt6331_upmu_get_qi_vtcxo1_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vtcxo1_mode = %d\n", mt6331_upmu_get_qi_vtcxo1_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vtcxo1_mode = %d\n", mt6331_upmu_get_qi_vtcxo1_mode()); 
}

void exec_6331_ldo_vtcxo1_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vtcxo1_on_ctrl(0);
    mt6331_upmu_set_rg_vtcxo1_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vtcxo1_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vtcxo1_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vtcxo1_lp_set(0);\n");
                        mt6331_upmu_set_rg_vtcxo1_lp_set(0);
                printk("mt6331_upmu_get_qi_vtcxo1_mode = %d\n", mt6331_upmu_get_qi_vtcxo1_mode());

                printk("mt6331_upmu_set_rg_vtcxo1_lp_set(1);\n");
                        mt6331_upmu_set_rg_vtcxo1_lp_set(1);
                printk("mt6331_upmu_get_qi_vtcxo1_mode = %d\n", mt6331_upmu_get_qi_vtcxo1_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vtcxo1_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vtcxo1_srclk_mode_sel(0); 
                exec_6331_ldo_vtcxo1_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vtcxo1_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vtcxo1_srclk_mode_sel(1);
                exec_6331_ldo_vtcxo1_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vtcxo1_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vtcxo1_srclk_mode_sel(2);
                exec_6331_ldo_vtcxo1_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vtcxo1_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vtcxo1_srclk_mode_sel(3);
                exec_6331_ldo_vtcxo1_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vtcxo1_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vtcxo1_lp_ctrl(0);
}

void exec_6331_ldo_vtcxo2_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vtcxo2_mode = %d\n", mt6331_upmu_get_qi_vtcxo2_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vtcxo2_mode = %d\n", mt6331_upmu_get_qi_vtcxo2_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vtcxo2_mode = %d\n", mt6331_upmu_get_qi_vtcxo2_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vtcxo2_mode = %d\n", mt6331_upmu_get_qi_vtcxo2_mode()); 
}

void exec_6331_ldo_vtcxo2_mode_test(void)
{
    int i=0;

    mt6331_upmu_set_rg_vtcxo2_on_ctrl(0);
    mt6331_upmu_set_rg_vtcxo2_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vtcxo2_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vtcxo2_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vtcxo2_lp_set(0);\n");
                        mt6331_upmu_set_rg_vtcxo2_lp_set(0);
                printk("mt6331_upmu_get_qi_vtcxo2_mode = %d\n", mt6331_upmu_get_qi_vtcxo2_mode());

                printk("mt6331_upmu_set_rg_vtcxo2_lp_set(1);\n");
                        mt6331_upmu_set_rg_vtcxo2_lp_set(1);
                printk("mt6331_upmu_get_qi_vtcxo2_mode = %d\n", mt6331_upmu_get_qi_vtcxo2_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vtcxo2_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vtcxo2_srclk_mode_sel(0); 
                exec_6331_ldo_vtcxo2_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vtcxo2_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vtcxo2_srclk_mode_sel(1);
                exec_6331_ldo_vtcxo2_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vtcxo2_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vtcxo2_srclk_mode_sel(2);
                exec_6331_ldo_vtcxo2_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vtcxo2_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vtcxo2_srclk_mode_sel(3);
                exec_6331_ldo_vtcxo2_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vtcxo2_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vtcxo2_lp_ctrl(0);
}

void exec_6331_ldo_vaud32_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vaud32_mode = %d\n", mt6331_upmu_get_qi_vaud32_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vaud32_mode = %d\n", mt6331_upmu_get_qi_vaud32_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vaud32_mode = %d\n", mt6331_upmu_get_qi_vaud32_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vaud32_mode = %d\n", mt6331_upmu_get_qi_vaud32_mode()); 
}

void exec_6331_ldo_vaud32_mode_test(void)
{
    int i=0;   

    mt6331_upmu_set_rg_vaud32_on_ctrl(0);
    mt6331_upmu_set_rg_vaud32_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vaud32_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vaud32_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vaud32_lp_set(0);\n");
                        mt6331_upmu_set_rg_vaud32_lp_set(0);
                printk("mt6331_upmu_get_qi_vaud32_mode = %d\n", mt6331_upmu_get_qi_vaud32_mode());

                printk("mt6331_upmu_set_rg_vaud32_lp_set(1);\n");
                        mt6331_upmu_set_rg_vaud32_lp_set(1);
                printk("mt6331_upmu_get_qi_vaud32_mode = %d\n", mt6331_upmu_get_qi_vaud32_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vaud32_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vaud32_srclk_mode_sel(0); 
                exec_6331_ldo_vaud32_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vaud32_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vaud32_srclk_mode_sel(1);
                exec_6331_ldo_vaud32_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vaud32_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vaud32_srclk_mode_sel(2);
                exec_6331_ldo_vaud32_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vaud32_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vaud32_srclk_mode_sel(3);
                exec_6331_ldo_vaud32_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vaud32_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vaud32_lp_ctrl(0);
}

void exec_6331_ldo_vauxa32_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vauxa32_mode = %d\n", mt6331_upmu_get_qi_vauxa32_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vauxa32_mode = %d\n", mt6331_upmu_get_qi_vauxa32_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vauxa32_mode = %d\n", mt6331_upmu_get_qi_vauxa32_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vauxa32_mode = %d\n", mt6331_upmu_get_qi_vauxa32_mode()); 
}

void exec_6331_ldo_vauxa32_mode_test(void)
{
    int i=0;   

    mt6331_upmu_set_rg_vauxa32_on_ctrl(0);
    mt6331_upmu_set_rg_vauxa32_en(1);

    printk("mt6331_upmu_set_rg_vauxa32_auxadc_pwdb_en(1);\n");
            mt6331_upmu_set_rg_vauxa32_auxadc_pwdb_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vauxa32_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vauxa32_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vauxa32_lp_set(0);\n");
                        mt6331_upmu_set_rg_vauxa32_lp_set(0);
                printk("mt6331_upmu_get_qi_vauxa32_mode = %d\n", mt6331_upmu_get_qi_vauxa32_mode());

                printk("mt6331_upmu_set_rg_vauxa32_lp_set(1);\n");
                        mt6331_upmu_set_rg_vauxa32_lp_set(1);
                printk("mt6331_upmu_get_qi_vauxa32_mode = %d\n", mt6331_upmu_get_qi_vauxa32_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vauxa32_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vauxa32_srclk_mode_sel(0); 
                exec_6331_ldo_vauxa32_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vauxa32_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vauxa32_srclk_mode_sel(1);
                exec_6331_ldo_vauxa32_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vauxa32_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vauxa32_srclk_mode_sel(2);
                exec_6331_ldo_vauxa32_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vauxa32_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vauxa32_srclk_mode_sel(3);
                exec_6331_ldo_vauxa32_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vauxa32_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vauxa32_lp_ctrl(0);
}

void exec_6331_ldo_vcama_mode_test(void)
{
    printk("no exec_6331_ldo_vcama_mode_test\n");
}

void exec_6331_ldo_vio28_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vio28_mode = %d\n", mt6331_upmu_get_qi_vio28_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vio28_mode = %d\n", mt6331_upmu_get_qi_vio28_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vio28_mode = %d\n", mt6331_upmu_get_qi_vio28_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vio28_mode = %d\n", mt6331_upmu_get_qi_vio28_mode()); 
}

void exec_6331_ldo_vio28_mode_test(void)
{
    int i=0;   

    mt6331_upmu_set_rg_vio28_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vio28_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vio28_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vio28_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vio28_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vio28_mode = %d\n", mt6331_upmu_get_qi_vio28_mode());

                printk("mt6331_upmu_set_rg_vio28_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vio28_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vio28_mode = %d\n", mt6331_upmu_get_qi_vio28_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vio28_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vio28_srclk_mode_sel(0); 
                exec_6331_ldo_vio28_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vio28_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vio28_srclk_mode_sel(1);
                exec_6331_ldo_vio28_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vio28_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vio28_srclk_mode_sel(2);
                exec_6331_ldo_vio28_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vio28_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vio28_srclk_mode_sel(3);
                exec_6331_ldo_vio28_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vio28_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vio28_lp_ctrl(0);
}

void exec_6331_ldo_vcam_af_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vcam_af_mode = %d\n", mt6331_upmu_get_qi_vcam_af_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vcam_af_mode = %d\n", mt6331_upmu_get_qi_vcam_af_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vcam_af_mode = %d\n", mt6331_upmu_get_qi_vcam_af_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vcam_af_mode = %d\n", mt6331_upmu_get_qi_vcam_af_mode()); 
}

void exec_6331_ldo_vcam_af_mode_test(void)
{
    int i=0;  

    mt6331_upmu_set_rg_vcam_af_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vcam_af_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vcam_af_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vcam_af_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vcam_af_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vcam_af_mode = %d\n", mt6331_upmu_get_qi_vcam_af_mode());

                printk("mt6331_upmu_set_rg_vcam_af_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vcam_af_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vcam_af_mode = %d\n", mt6331_upmu_get_qi_vcam_af_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vcam_af_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vcam_af_srclk_mode_sel(0); 
                exec_6331_ldo_vcam_af_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vcam_af_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vcam_af_srclk_mode_sel(1);
                exec_6331_ldo_vcam_af_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vcam_af_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vcam_af_srclk_mode_sel(2);
                exec_6331_ldo_vcam_af_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vcam_af_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vcam_af_srclk_mode_sel(3);
                exec_6331_ldo_vcam_af_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vcam_af_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vcam_af_lp_ctrl(0);
}

void exec_6331_ldo_vmc_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vmc_mode = %d\n", mt6331_upmu_get_qi_vmc_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vmc_mode = %d\n", mt6331_upmu_get_qi_vmc_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vmc_mode = %d\n", mt6331_upmu_get_qi_vmc_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vmc_mode = %d\n", mt6331_upmu_get_qi_vmc_mode()); 
}

void exec_6331_ldo_vmc_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vmc_on_ctrl(0);
    mt6331_upmu_set_rg_vmc_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vmc_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vmc_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vmc_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vmc_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vmc_mode = %d\n", mt6331_upmu_get_qi_vmc_mode());

                printk("mt6331_upmu_set_rg_vmc_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vmc_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vmc_mode = %d\n", mt6331_upmu_get_qi_vmc_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vmc_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vmc_srclk_mode_sel(0); 
                exec_6331_ldo_vmc_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vmc_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vmc_srclk_mode_sel(1);
                exec_6331_ldo_vmc_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vmc_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vmc_srclk_mode_sel(2);
                exec_6331_ldo_vmc_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vmc_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vmc_srclk_mode_sel(3);
                exec_6331_ldo_vmc_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vmc_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vmc_lp_ctrl(0);
}

void exec_6331_ldo_vmch_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vmch_mode = %d\n", mt6331_upmu_get_qi_vmch_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vmch_mode = %d\n", mt6331_upmu_get_qi_vmch_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vmch_mode = %d\n", mt6331_upmu_get_qi_vmch_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vmch_mode = %d\n", mt6331_upmu_get_qi_vmch_mode()); 
}

void exec_6331_ldo_vmch_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vmch_on_ctrl(0);
    mt6331_upmu_set_rg_vmch_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vmch_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vmch_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vmch_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vmch_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vmch_mode = %d\n", mt6331_upmu_get_qi_vmch_mode());

                printk("mt6331_upmu_set_rg_vmch_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vmch_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vmch_mode = %d\n", mt6331_upmu_get_qi_vmch_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vmch_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vmch_srclk_mode_sel(0); 
                exec_6331_ldo_vmch_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vmch_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vmch_srclk_mode_sel(1);
                exec_6331_ldo_vmch_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vmch_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vmch_srclk_mode_sel(2);
                exec_6331_ldo_vmch_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vmch_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vmch_srclk_mode_sel(3);
                exec_6331_ldo_vmch_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vmch_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vmch_lp_ctrl(0);
}

void exec_6331_ldo_vemc33_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vemc33_mode = %d\n", mt6331_upmu_get_qi_vemc33_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vemc33_mode = %d\n", mt6331_upmu_get_qi_vemc33_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vemc33_mode = %d\n", mt6331_upmu_get_qi_vemc33_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vemc33_mode = %d\n", mt6331_upmu_get_qi_vemc33_mode()); 
}

void exec_6331_ldo_vemc33_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vemc33_on_ctrl(0);
    mt6331_upmu_set_rg_vemc33_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vemc33_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vemc33_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vemc33_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vemc33_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vemc33_mode = %d\n", mt6331_upmu_get_qi_vemc33_mode());

                printk("mt6331_upmu_set_rg_vemc33_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vemc33_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vemc33_mode = %d\n", mt6331_upmu_get_qi_vemc33_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vemc33_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vemc33_srclk_mode_sel(0); 
                exec_6331_ldo_vemc33_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vemc33_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vemc33_srclk_mode_sel(1);
                exec_6331_ldo_vemc33_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vemc33_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vemc33_srclk_mode_sel(2);
                exec_6331_ldo_vemc33_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vemc33_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vemc33_srclk_mode_sel(3);
                exec_6331_ldo_vemc33_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vemc33_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vemc33_lp_ctrl(0);
}

void exec_6331_ldo_vgp1_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vgp1_mode = %d\n", mt6331_upmu_get_qi_vgp1_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vgp1_mode = %d\n", mt6331_upmu_get_qi_vgp1_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vgp1_mode = %d\n", mt6331_upmu_get_qi_vgp1_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vgp1_mode = %d\n", mt6331_upmu_get_qi_vgp1_mode()); 
}

void exec_6331_ldo_vgp1_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vgp1_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vgp1_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vgp1_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vgp1_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vgp1_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vgp1_mode = %d\n", mt6331_upmu_get_qi_vgp1_mode());

                printk("mt6331_upmu_set_rg_vgp1_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vgp1_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vgp1_mode = %d\n", mt6331_upmu_get_qi_vgp1_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vgp1_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vgp1_srclk_mode_sel(0); 
                exec_6331_ldo_vgp1_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vgp1_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vgp1_srclk_mode_sel(1);
                exec_6331_ldo_vgp1_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vgp1_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vgp1_srclk_mode_sel(2);
                exec_6331_ldo_vgp1_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vgp1_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vgp1_srclk_mode_sel(3);
                exec_6331_ldo_vgp1_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vgp1_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vgp1_lp_ctrl(0);
}

void exec_6331_ldo_vgp4_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vgp4_mode = %d\n", mt6331_upmu_get_qi_vgp4_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vgp4_mode = %d\n", mt6331_upmu_get_qi_vgp4_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vgp4_mode = %d\n", mt6331_upmu_get_qi_vgp4_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vgp4_mode = %d\n", mt6331_upmu_get_qi_vgp4_mode()); 
}

void exec_6331_ldo_vgp4_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vgp4_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vgp4_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vgp4_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vgp4_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vgp4_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vgp4_mode = %d\n", mt6331_upmu_get_qi_vgp4_mode());

                printk("mt6331_upmu_set_rg_vgp4_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vgp4_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vgp4_mode = %d\n", mt6331_upmu_get_qi_vgp4_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vgp4_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vgp4_srclk_mode_sel(0); 
                exec_6331_ldo_vgp4_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vgp4_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vgp4_srclk_mode_sel(1);
                exec_6331_ldo_vgp4_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vgp4_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vgp4_srclk_mode_sel(2);
                exec_6331_ldo_vgp4_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vgp4_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vgp4_srclk_mode_sel(3);
                exec_6331_ldo_vgp4_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vgp4_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vgp4_lp_ctrl(0);
}

void exec_6331_ldo_vsim1_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vsim1_mode = %d\n", mt6331_upmu_get_qi_vsim1_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vsim1_mode = %d\n", mt6331_upmu_get_qi_vsim1_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vsim1_mode = %d\n", mt6331_upmu_get_qi_vsim1_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vsim1_mode = %d\n", mt6331_upmu_get_qi_vsim1_mode()); 
}

void exec_6331_ldo_vsim1_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vsim1_on_ctrl(0);
    mt6331_upmu_set_rg_vsim1_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vsim1_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vsim1_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vsim1_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vsim1_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vsim1_mode = %d\n", mt6331_upmu_get_qi_vsim1_mode());

                printk("mt6331_upmu_set_rg_vsim1_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vsim1_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vsim1_mode = %d\n", mt6331_upmu_get_qi_vsim1_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vsim1_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vsim1_srclk_mode_sel(0); 
                exec_6331_ldo_vsim1_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vsim1_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vsim1_srclk_mode_sel(1);
                exec_6331_ldo_vsim1_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vsim1_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vsim1_srclk_mode_sel(2);
                exec_6331_ldo_vsim1_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vsim1_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vsim1_srclk_mode_sel(3);
                exec_6331_ldo_vsim1_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vsim1_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vsim1_lp_ctrl(0);
}

void exec_6331_ldo_vsim2_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vsim2_mode = %d\n", mt6331_upmu_get_qi_vsim2_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vsim2_mode = %d\n", mt6331_upmu_get_qi_vsim2_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vsim2_mode = %d\n", mt6331_upmu_get_qi_vsim2_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vsim2_mode = %d\n", mt6331_upmu_get_qi_vsim2_mode()); 
}

void exec_6331_ldo_vsim2_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vsim2_on_ctrl(0);
    mt6331_upmu_set_rg_vsim2_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vsim2_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vsim2_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vsim2_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vsim2_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vsim2_mode = %d\n", mt6331_upmu_get_qi_vsim2_mode());

                printk("mt6331_upmu_set_rg_vsim2_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vsim2_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vsim2_mode = %d\n", mt6331_upmu_get_qi_vsim2_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vsim2_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vsim2_srclk_mode_sel(0); 
                exec_6331_ldo_vsim2_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vsim2_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vsim2_srclk_mode_sel(1);
                exec_6331_ldo_vsim2_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vsim2_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vsim2_srclk_mode_sel(2);
                exec_6331_ldo_vsim2_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vsim2_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vsim2_srclk_mode_sel(3);
                exec_6331_ldo_vsim2_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vsim2_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vsim2_lp_ctrl(0);
}

void exec_6331_ldo_vfbb_mode_test(void)
{
    printk("no exec_6331_ldo_vfbb_mode_test\n");
}

void exec_6331_ldo_vibr_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vibr_mode = %d\n", mt6331_upmu_get_qi_vibr_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vibr_mode = %d\n", mt6331_upmu_get_qi_vibr_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vibr_mode = %d\n", mt6331_upmu_get_qi_vibr_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vibr_mode = %d\n", mt6331_upmu_get_qi_vibr_mode()); 
}

void exec_6331_ldo_vibr_mode_test(void)
{
    int i=0;  

    mt6331_upmu_set_rg_vibr_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vibr_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vibr_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vibr_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vibr_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vibr_mode = %d\n", mt6331_upmu_get_qi_vibr_mode());

                printk("mt6331_upmu_set_rg_vibr_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vibr_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vibr_mode = %d\n", mt6331_upmu_get_qi_vibr_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vibr_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vibr_srclk_mode_sel(0); 
                exec_6331_ldo_vibr_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vibr_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vibr_srclk_mode_sel(1);
                exec_6331_ldo_vibr_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vibr_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vibr_srclk_mode_sel(2);
                exec_6331_ldo_vibr_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vibr_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vibr_srclk_mode_sel(3);
                exec_6331_ldo_vibr_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vibr_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vibr_lp_ctrl(0);
}

void exec_6331_ldo_vrtc_mode_test(void)
{
    printk("no exec_6331_ldo_vrtc_mode_test\n");
}

void exec_6331_ldo_vdig18_mode_test(void)
{
    printk("no exec_6331_ldo_vdig18_mode_test\n");
}

void exec_6331_ldo_vmipi_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vmipi_mode = %d\n", mt6331_upmu_get_qi_vmipi_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vmipi_mode = %d\n", mt6331_upmu_get_qi_vmipi_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vmipi_mode = %d\n", mt6331_upmu_get_qi_vmipi_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vmipi_mode = %d\n", mt6331_upmu_get_qi_vmipi_mode()); 
}

void exec_6331_ldo_vmipi_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vmipi_on_ctrl(0);
    mt6331_upmu_set_rg_vmipi_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vmipi_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vmipi_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vmipi_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vmipi_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vmipi_mode = %d\n", mt6331_upmu_get_qi_vmipi_mode());

                printk("mt6331_upmu_set_rg_vmipi_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vmipi_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vmipi_mode = %d\n", mt6331_upmu_get_qi_vmipi_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vmipi_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vmipi_srclk_mode_sel(0); 
                exec_6331_ldo_vmipi_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vmipi_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vmipi_srclk_mode_sel(1);
                exec_6331_ldo_vmipi_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vmipi_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vmipi_srclk_mode_sel(2);
                exec_6331_ldo_vmipi_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vmipi_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vmipi_srclk_mode_sel(3);
                exec_6331_ldo_vmipi_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vmipi_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vmipi_lp_ctrl(0);
}

void exec_6331_ldo_vcamd_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vcamd_mode = %d\n", mt6331_upmu_get_qi_vcamd_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vcamd_mode = %d\n", mt6331_upmu_get_qi_vcamd_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vcamd_mode = %d\n", mt6331_upmu_get_qi_vcamd_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vcamd_mode = %d\n", mt6331_upmu_get_qi_vcamd_mode()); 
}

void exec_6331_ldo_vcamd_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vcamd_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vcamd_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vcamd_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vcamd_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vcamd_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vcamd_mode = %d\n", mt6331_upmu_get_qi_vcamd_mode());

                printk("mt6331_upmu_set_rg_vcamd_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vcamd_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vcamd_mode = %d\n", mt6331_upmu_get_qi_vcamd_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vcamd_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vcamd_srclk_mode_sel(0); 
                exec_6331_ldo_vcamd_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vcamd_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vcamd_srclk_mode_sel(1);
                exec_6331_ldo_vcamd_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vcamd_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vcamd_srclk_mode_sel(2);
                exec_6331_ldo_vcamd_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vcamd_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vcamd_srclk_mode_sel(3);
                exec_6331_ldo_vcamd_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vcamd_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vcamd_lp_ctrl(0);
}

void exec_6331_ldo_vusb10_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vusb10_mode = %d\n", mt6331_upmu_get_qi_vusb10_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vusb10_mode = %d\n", mt6331_upmu_get_qi_vusb10_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vusb10_mode = %d\n", mt6331_upmu_get_qi_vusb10_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vusb10_mode = %d\n", mt6331_upmu_get_qi_vusb10_mode()); 
}

void exec_6331_ldo_vusb10_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vusb10_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vusb10_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vusb10_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vusb10_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vusb10_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vusb10_mode = %d\n", mt6331_upmu_get_qi_vusb10_mode());

                printk("mt6331_upmu_set_rg_vusb10_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vusb10_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vusb10_mode = %d\n", mt6331_upmu_get_qi_vusb10_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vusb10_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vusb10_srclk_mode_sel(0); 
                exec_6331_ldo_vusb10_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vusb10_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vusb10_srclk_mode_sel(1);
                exec_6331_ldo_vusb10_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vusb10_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vusb10_srclk_mode_sel(2);
                exec_6331_ldo_vusb10_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vusb10_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vusb10_srclk_mode_sel(3);
                exec_6331_ldo_vusb10_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vusb10_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vusb10_lp_ctrl(0);
}

void exec_6331_ldo_vcam_io_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vcam_io_mode = %d\n", mt6331_upmu_get_qi_vcam_io_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vcam_io_mode = %d\n", mt6331_upmu_get_qi_vcam_io_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vcam_io_mode = %d\n", mt6331_upmu_get_qi_vcam_io_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vcam_io_mode = %d\n", mt6331_upmu_get_qi_vcam_io_mode()); 
}

void exec_6331_ldo_vcam_io_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vcam_io_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vcam_io_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vcam_io_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vcam_io_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vcam_io_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vcam_io_mode = %d\n", mt6331_upmu_get_qi_vcam_io_mode());

                printk("mt6331_upmu_set_rg_vcam_io_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vcam_io_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vcam_io_mode = %d\n", mt6331_upmu_get_qi_vcam_io_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vcam_io_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vcam_io_srclk_mode_sel(0); 
                exec_6331_ldo_vcam_io_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vcam_io_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vcam_io_srclk_mode_sel(1);
                exec_6331_ldo_vcam_io_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vcam_io_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vcam_io_srclk_mode_sel(2);
                exec_6331_ldo_vcam_io_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vcam_io_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vcam_io_srclk_mode_sel(3);
                exec_6331_ldo_vcam_io_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vcam_io_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vcam_io_lp_ctrl(0);
}

void exec_6331_ldo_vsram_dvfs1_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vsram_dvfs1_mode = %d\n", mt6331_upmu_get_qi_vsram_dvfs1_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vsram_dvfs1_mode = %d\n", mt6331_upmu_get_qi_vsram_dvfs1_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vsram_dvfs1_mode = %d\n", mt6331_upmu_get_qi_vsram_dvfs1_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vsram_dvfs1_mode = %d\n", mt6331_upmu_get_qi_vsram_dvfs1_mode()); 
}

void exec_6331_ldo_vsram_dvfs1_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vsram_dvfs1_on_ctrl(0);
    mt6331_upmu_set_rg_vsram_dvfs1_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vsram_dvfs1_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vsram_dvfs1_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vsram_dvfs1_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vsram_dvfs1_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vsram_dvfs1_mode = %d\n", mt6331_upmu_get_qi_vsram_dvfs1_mode());

                printk("mt6331_upmu_set_rg_vsram_dvfs1_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vsram_dvfs1_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vsram_dvfs1_mode = %d\n", mt6331_upmu_get_qi_vsram_dvfs1_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vsram_dvfs1_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vsram_dvfs1_srclk_mode_sel(0); 
                exec_6331_ldo_vsram_dvfs1_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vsram_dvfs1_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vsram_dvfs1_srclk_mode_sel(1);
                exec_6331_ldo_vsram_dvfs1_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vsram_dvfs1_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vsram_dvfs1_srclk_mode_sel(2);
                exec_6331_ldo_vsram_dvfs1_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vsram_dvfs1_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vsram_dvfs1_srclk_mode_sel(3);
                exec_6331_ldo_vsram_dvfs1_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vsram_dvfs1_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vsram_dvfs1_lp_ctrl(0);
}

void exec_6331_ldo_vgp2_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vgp2_mode = %d\n", mt6331_upmu_get_qi_vgp2_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vgp2_mode = %d\n", mt6331_upmu_get_qi_vgp2_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vgp2_mode = %d\n", mt6331_upmu_get_qi_vgp2_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vgp2_mode = %d\n", mt6331_upmu_get_qi_vgp2_mode()); 
}

void exec_6331_ldo_vgp2_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vgp2_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vgp2_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vgp2_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vgp2_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vgp2_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vgp2_mode = %d\n", mt6331_upmu_get_qi_vgp2_mode());

                printk("mt6331_upmu_set_rg_vgp2_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vgp2_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vgp2_mode = %d\n", mt6331_upmu_get_qi_vgp2_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vgp2_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vgp2_srclk_mode_sel(0); 
                exec_6331_ldo_vgp2_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vgp2_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vgp2_srclk_mode_sel(1);
                exec_6331_ldo_vgp2_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vgp2_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vgp2_srclk_mode_sel(2);
                exec_6331_ldo_vgp2_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vgp2_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vgp2_srclk_mode_sel(3);
                exec_6331_ldo_vgp2_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vgp2_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vgp2_lp_ctrl(0);
}

void exec_6331_ldo_vgp3_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vgp3_mode = %d\n", mt6331_upmu_get_qi_vgp3_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vgp3_mode = %d\n", mt6331_upmu_get_qi_vgp3_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vgp3_mode = %d\n", mt6331_upmu_get_qi_vgp3_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vgp3_mode = %d\n", mt6331_upmu_get_qi_vgp3_mode()); 
}

void exec_6331_ldo_vgp3_mode_test(void)
{
    int i=0;    

    mt6331_upmu_set_rg_vgp3_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vgp3_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vgp3_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vgp3_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vgp3_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vgp3_mode = %d\n", mt6331_upmu_get_qi_vgp3_mode());

                printk("mt6331_upmu_set_rg_vgp3_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vgp3_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vgp3_mode = %d\n", mt6331_upmu_get_qi_vgp3_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vgp3_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vgp3_srclk_mode_sel(0); 
                exec_6331_ldo_vgp3_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vgp3_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vgp3_srclk_mode_sel(1);
                exec_6331_ldo_vgp3_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vgp3_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vgp3_srclk_mode_sel(2);
                exec_6331_ldo_vgp3_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vgp3_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vgp3_srclk_mode_sel(3);
                exec_6331_ldo_vgp3_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vgp3_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vgp3_lp_ctrl(0);
}

void exec_6331_ldo_vbiasn_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6331_upmu_get_qi_vbiasn_mode = %d\n", mt6331_upmu_get_qi_vbiasn_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6331_upmu_get_qi_vbiasn_mode = %d\n", mt6331_upmu_get_qi_vbiasn_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6331_upmu_get_qi_vbiasn_mode = %d\n", mt6331_upmu_get_qi_vbiasn_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6331_upmu_get_qi_vbiasn_mode = %d\n", mt6331_upmu_get_qi_vbiasn_mode()); 
}

void exec_6331_ldo_vbiasn_mode_test(void)
{
    int i=0;   

    mt6331_upmu_set_rg_vbiasn_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6331_upmu_set_rg_vbiasn_lp_ctrl] %d\n", i);
        mt6331_upmu_set_rg_vbiasn_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6331_upmu_set_rg_vbiasn_lp_mode_set(0);\n");
                        mt6331_upmu_set_rg_vbiasn_lp_mode_set(0);
                printk("mt6331_upmu_get_qi_vbiasn_mode = %d\n", mt6331_upmu_get_qi_vbiasn_mode());

                printk("mt6331_upmu_set_rg_vbiasn_lp_mode_set(1);\n");
                        mt6331_upmu_set_rg_vbiasn_lp_mode_set(1);
                printk("mt6331_upmu_get_qi_vbiasn_mode = %d\n", mt6331_upmu_get_qi_vbiasn_mode());
                break;

            case 1:
                printk("[mt6331_upmu_set_rg_vbiasn_srclk_mode_sel(0)]\n"); 
                         mt6331_upmu_set_rg_vbiasn_srclk_mode_sel(0); 
                exec_6331_ldo_vbiasn_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vbiasn_srclk_mode_sel(1)]\n");
                         mt6331_upmu_set_rg_vbiasn_srclk_mode_sel(1);
                exec_6331_ldo_vbiasn_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vbiasn_srclk_mode_sel(2)]\n");
                         mt6331_upmu_set_rg_vbiasn_srclk_mode_sel(2);
                exec_6331_ldo_vbiasn_mode_test_sub();
                
                printk("[mt6331_upmu_set_rg_vbiasn_srclk_mode_sel(3)]\n");
                         mt6331_upmu_set_rg_vbiasn_srclk_mode_sel(3);
                exec_6331_ldo_vbiasn_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6331_upmu_set_rg_vbiasn_lp_ctrl(0);\n"); mt6331_upmu_set_rg_vbiasn_lp_ctrl(0);
}

void exec_6332_ldo_vauxb32_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6332_upmu_get_qi_vauxb32_mode = %d\n", mt6332_upmu_get_qi_vauxb32_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6332_upmu_get_qi_vauxb32_mode = %d\n", mt6332_upmu_get_qi_vauxb32_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6332_upmu_get_qi_vauxb32_mode = %d\n", mt6332_upmu_get_qi_vauxb32_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6332_upmu_get_qi_vauxb32_mode = %d\n", mt6332_upmu_get_qi_vauxb32_mode()); 
}

void exec_6332_ldo_vauxb32_mode_test(void)
{
    int i=0;

    mt6332_upmu_set_rg_vauxb32_on_ctrl(0);
    mt6332_upmu_set_rg_vauxb32_en(1);

    printk("mt6332_upmu_set_rg_vauxb32_auxadc_pwdb_en(1);\n");
            mt6332_upmu_set_rg_vauxb32_auxadc_pwdb_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_rg_vauxb32_lp_ctrl] %d\n", i);
        mt6332_upmu_set_rg_vauxb32_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6332_upmu_set_rg_vauxb32_lp_mode_set(0);\n");
                        mt6332_upmu_set_rg_vauxb32_lp_mode_set(0);
                printk("mt6332_upmu_get_qi_vauxb32_mode = %d\n", mt6332_upmu_get_qi_vauxb32_mode());

                printk("mt6332_upmu_set_rg_vauxb32_lp_mode_set(1);\n");
                        mt6332_upmu_set_rg_vauxb32_lp_mode_set(1);
                printk("mt6332_upmu_get_qi_vauxb32_mode = %d\n", mt6332_upmu_get_qi_vauxb32_mode());
                break;

            case 1:
                printk("[mt6332_upmu_set_rg_vauxb32_srclk_mode_sel(0)]\n"); 
                         mt6332_upmu_set_rg_vauxb32_srclk_mode_sel(0); 
                exec_6332_ldo_vauxb32_mode_test_sub();
                
                printk("[mt6332_upmu_set_rg_vauxb32_srclk_mode_sel(1)]\n");
                         mt6332_upmu_set_rg_vauxb32_srclk_mode_sel(1);
                exec_6332_ldo_vauxb32_mode_test_sub();
                
                printk("[mt6332_upmu_set_rg_vauxb32_srclk_mode_sel(2)]\n");
                         mt6332_upmu_set_rg_vauxb32_srclk_mode_sel(2);
                exec_6332_ldo_vauxb32_mode_test_sub();
                
                printk("[mt6332_upmu_set_rg_vauxb32_srclk_mode_sel(3)]\n");
                         mt6332_upmu_set_rg_vauxb32_srclk_mode_sel(3);
                exec_6332_ldo_vauxb32_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6332_upmu_set_rg_vauxb32_lp_ctrl(0);\n"); mt6332_upmu_set_rg_vauxb32_lp_ctrl(0);
}

void exec_6332_ldo_vbif28_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6332_upmu_get_qi_vbif28_mode = %d\n", mt6332_upmu_get_qi_vbif28_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6332_upmu_get_qi_vbif28_mode = %d\n", mt6332_upmu_get_qi_vbif28_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6332_upmu_get_qi_vbif28_mode = %d\n", mt6332_upmu_get_qi_vbif28_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6332_upmu_get_qi_vbif28_mode = %d\n", mt6332_upmu_get_qi_vbif28_mode()); 
}

void exec_6332_ldo_vbif28_mode_test(void)
{
    int i=0;  

    mt6332_upmu_set_rg_vbif28_on_ctrl(0);
    mt6332_upmu_set_rg_vbif28_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_rg_vbif28_lp_ctrl] %d\n", i);
        mt6332_upmu_set_rg_vbif28_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6332_upmu_set_rg_vbif28_lp_mode_set(0);\n");
                        mt6332_upmu_set_rg_vbif28_lp_mode_set(0);
                printk("mt6332_upmu_get_qi_vbif28_mode = %d\n", mt6332_upmu_get_qi_vbif28_mode());

                printk("mt6332_upmu_set_rg_vbif28_lp_mode_set(1);\n");
                        mt6332_upmu_set_rg_vbif28_lp_mode_set(1);
                printk("mt6332_upmu_get_qi_vbif28_mode = %d\n", mt6332_upmu_get_qi_vbif28_mode());
                break;

            case 1:
                printk("[mt6332_upmu_set_rg_vbif28_srclk_mode_sel(0)]\n"); 
                         mt6332_upmu_set_rg_vbif28_srclk_mode_sel(0); 
                exec_6332_ldo_vbif28_mode_test_sub();
                
                printk("[mt6332_upmu_set_rg_vbif28_srclk_mode_sel(1)]\n");
                         mt6332_upmu_set_rg_vbif28_srclk_mode_sel(1);
                exec_6332_ldo_vbif28_mode_test_sub();
                
                printk("[mt6332_upmu_set_rg_vbif28_srclk_mode_sel(2)]\n");
                         mt6332_upmu_set_rg_vbif28_srclk_mode_sel(2);
                exec_6332_ldo_vbif28_mode_test_sub();
                
                printk("[mt6332_upmu_set_rg_vbif28_srclk_mode_sel(3)]\n");
                         mt6332_upmu_set_rg_vbif28_srclk_mode_sel(3);
                exec_6332_ldo_vbif28_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6332_upmu_set_rg_vbif28_lp_ctrl(0);\n"); mt6332_upmu_set_rg_vbif28_lp_ctrl(0);
}

void exec_6332_ldo_vusb33_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6332_upmu_get_qi_vusb33_mode = %d\n", mt6332_upmu_get_qi_vusb33_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6332_upmu_get_qi_vusb33_mode = %d\n", mt6332_upmu_get_qi_vusb33_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6332_upmu_get_qi_vusb33_mode = %d\n", mt6332_upmu_get_qi_vusb33_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6332_upmu_get_qi_vusb33_mode = %d\n", mt6332_upmu_get_qi_vusb33_mode()); 
}

void exec_6332_ldo_vusb33_mode_test(void)
{
    int i=0;   

    mt6332_upmu_set_rg_vusb33_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_rg_vusb33_lp_ctrl] %d\n", i);
        mt6332_upmu_set_rg_vusb33_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6332_upmu_set_rg_vusb33_lp_mode_set(0);\n");
                        mt6332_upmu_set_rg_vusb33_lp_mode_set(0);
                printk("mt6332_upmu_get_qi_vusb33_mode = %d\n", mt6332_upmu_get_qi_vusb33_mode());

                printk("mt6332_upmu_set_rg_vusb33_lp_mode_set(1);\n");
                        mt6332_upmu_set_rg_vusb33_lp_mode_set(1);
                printk("mt6332_upmu_get_qi_vusb33_mode = %d\n", mt6332_upmu_get_qi_vusb33_mode());
                break;

            case 1:
                printk("[mt6332_upmu_set_rg_vusb33_srclk_mode_sel(0)]\n"); 
                         mt6332_upmu_set_rg_vusb33_srclk_mode_sel(0); 
                exec_6332_ldo_vusb33_mode_test_sub();
                
                printk("[mt6332_upmu_set_rg_vusb33_srclk_mode_sel(1)]\n");
                         mt6332_upmu_set_rg_vusb33_srclk_mode_sel(1);
                exec_6332_ldo_vusb33_mode_test_sub();
                
                printk("[mt6332_upmu_set_rg_vusb33_srclk_mode_sel(2)]\n");
                         mt6332_upmu_set_rg_vusb33_srclk_mode_sel(2);
                exec_6332_ldo_vusb33_mode_test_sub();
                
                printk("[mt6332_upmu_set_rg_vusb33_srclk_mode_sel(3)]\n");
                         mt6332_upmu_set_rg_vusb33_srclk_mode_sel(3);
                exec_6332_ldo_vusb33_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6332_upmu_set_rg_vusb33_lp_ctrl(0);\n"); mt6332_upmu_set_rg_vusb33_lp_ctrl(0);
}

void exec_6332_ldo_vdig18_mode_test(void)
{
    printk("no exec_6332_ldo_vdig18_mode_test\n");
}

void exec_6332_ldo_vsram_dvfs2_mode_test_sub(void)
{
    set_srclken_sw_mode();

    printk("set_srclken_1_val(0); set_srclken_2_val(0);\n"); set_srclken_1_val(0); set_srclken_2_val(0);
    printk("mt6332_upmu_get_qi_vsram_dvfs2_mode = %d\n", mt6332_upmu_get_qi_vsram_dvfs2_mode()); 
    printk("set_srclken_1_val(0); set_srclken_2_val(1);\n"); set_srclken_1_val(0); set_srclken_2_val(1);
    printk("mt6332_upmu_get_qi_vsram_dvfs2_mode = %d\n", mt6332_upmu_get_qi_vsram_dvfs2_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(0);\n"); set_srclken_1_val(1); set_srclken_2_val(0); 
    printk("mt6332_upmu_get_qi_vsram_dvfs2_mode = %d\n", mt6332_upmu_get_qi_vsram_dvfs2_mode()); 
    printk("set_srclken_1_val(1); set_srclken_2_val(1);\n"); set_srclken_1_val(1); set_srclken_2_val(1); 
    printk("mt6332_upmu_get_qi_vsram_dvfs2_mode = %d\n", mt6332_upmu_get_qi_vsram_dvfs2_mode()); 
}

void exec_6332_ldo_vsram_dvfs2_mode_test(void)
{
    int i=0;  

    mt6332_upmu_set_rg_vsram_dvfs2_on_ctrl(0);
    mt6332_upmu_set_rg_vsram_dvfs2_en(1);

    for(i=0;i<=1;i++)
    {
        printk("[mt6332_upmu_set_rg_vsram_dvfs2_lp_ctrl] %d\n", i);
        mt6332_upmu_set_rg_vsram_dvfs2_lp_ctrl(i);

        switch(i){
            case 0:
                printk("mt6332_upmu_set_rg_vsram_dvfs2_lp_mode_set(0);\n");
                        mt6332_upmu_set_rg_vsram_dvfs2_lp_mode_set(0);
                printk("mt6332_upmu_get_qi_vsram_dvfs2_mode = %d\n", mt6332_upmu_get_qi_vsram_dvfs2_mode());

                printk("mt6332_upmu_set_rg_vsram_dvfs2_lp_mode_set(1);\n");
                        mt6332_upmu_set_rg_vsram_dvfs2_lp_mode_set(1);
                printk("mt6332_upmu_get_qi_vsram_dvfs2_mode = %d\n", mt6332_upmu_get_qi_vsram_dvfs2_mode());
                break;

            case 1:
                printk("[mt6332_upmu_set_rg_vsram_dvfs2_srclk_mode_sel(0)]\n"); 
                         mt6332_upmu_set_rg_vsram_dvfs2_srclk_mode_sel(0); 
                exec_6332_ldo_vsram_dvfs2_mode_test_sub();
                
                printk("[mt6332_upmu_set_rg_vsram_dvfs2_srclk_mode_sel(1)]\n");
                         mt6332_upmu_set_rg_vsram_dvfs2_srclk_mode_sel(1);
                exec_6332_ldo_vsram_dvfs2_mode_test_sub();
                
                printk("[mt6332_upmu_set_rg_vsram_dvfs2_srclk_mode_sel(2)]\n");
                         mt6332_upmu_set_rg_vsram_dvfs2_srclk_mode_sel(2);
                exec_6332_ldo_vsram_dvfs2_mode_test_sub();
                
                printk("[mt6332_upmu_set_rg_vsram_dvfs2_srclk_mode_sel(3)]\n");
                         mt6332_upmu_set_rg_vsram_dvfs2_srclk_mode_sel(3);
                exec_6332_ldo_vsram_dvfs2_mode_test_sub();  
                break;

            default:
                printk("Invalid channel value(%d)\n", i);
                break;
        }
    }

    printk("Final reset to 0: mt6332_upmu_set_rg_vsram_dvfs2_lp_ctrl(0);\n"); mt6332_upmu_set_rg_vsram_dvfs2_lp_ctrl(0);
}
#endif

//////////////////////////////////////////
// BIF
//////////////////////////////////////////
#ifdef BIF_DVT_ENABLE
int g_loop_out = 50;

void reset_bif_irq(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int loop_i=0;
    
    printk("reset BIF_IRQ : BIF_IRQ_CLR=1\n");
    ret=pmic_config_interface(MT6332_BIF_CON31, 0x1, 0x1, 1);
    
    reg_val=0;
    while(reg_val != 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    printk("When BIF_IRQ=0 : BIF_IRQ_CLR =0\n");
    ret=pmic_config_interface(MT6332_BIF_CON31, 0x0, 0x1, 1); 
}

void set_bif_cmd(int bif_cmd[], int bif_cmd_len)
{
    int i=0;
    int con_index=0;
    U32 ret=0;
    
    for(i=0;i<bif_cmd_len;i++)
    {
        ret=pmic_config_interface(MT6332_BIF_CON0+con_index, bif_cmd[i], 0x07FF, 0);
        con_index += 0x2;
    }
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "\n Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x \n Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x \n Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON0, upmu_get_reg_value(MT6332_BIF_CON0),
        MT6332_BIF_CON1, upmu_get_reg_value(MT6332_BIF_CON1),
        MT6332_BIF_CON2, upmu_get_reg_value(MT6332_BIF_CON2),
        MT6332_BIF_CON3, upmu_get_reg_value(MT6332_BIF_CON3),
        MT6332_BIF_CON4, upmu_get_reg_value(MT6332_BIF_CON4),
        MT6332_BIF_CON5, upmu_get_reg_value(MT6332_BIF_CON5),
        MT6332_BIF_CON6, upmu_get_reg_value(MT6332_BIF_CON6),
        MT6332_BIF_CON7, upmu_get_reg_value(MT6332_BIF_CON7),
        MT6332_BIF_CON8, upmu_get_reg_value(MT6332_BIF_CON8),
        MT6332_BIF_CON9, upmu_get_reg_value(MT6332_BIF_CON9),
        MT6332_BIF_CON10,upmu_get_reg_value(MT6332_BIF_CON10),
        MT6332_BIF_CON11,upmu_get_reg_value(MT6332_BIF_CON11)
        );
}

void check_bat_lost(void)
{
    U32 ret=0;
    U32 reg_val=0;
        
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 13);
    printk("[check_bat_lost] BIF_BAT_LOST : BIF_CON31[13]=0x%x - battery is detected(0)/battery is undetected(1)\n", reg_val);
}

void bif_debug(void)
{
    U32 ret=0;    
    U32 qi_bif_tx_en=0;
    U32 qi_bif_tx_data=0;
    U32 qi_bif_rx_en=0;
    U32 qi_bif_rx_data=0;
    U32 bif_tx_state=0;
    U32 bif_flow_ctl_state=0;    
    U32 bif_rx_state=0;
    U32 bif_data_num=0;
    U32 rgs_baton_ht=0;
    U32 repeat=10;
    int i=0;

    printk("[bif_debug] qi_bif_tx_en, qi_bif_tx_data, qi_bif_rx_en, qi_bif_rx_data, bif_tx_state, bif_flow_ctl_state, bif_rx_state, bif_data_num, rgs_baton_ht\n");

    for(i=0;i<repeat;i++)
    {
        ret=pmic_read_interface(MT6332_BIF_CON33, &qi_bif_tx_en, 0x1, 15);
        ret=pmic_read_interface(MT6332_BIF_CON33, &qi_bif_tx_data, 0x1, 14);
        ret=pmic_read_interface(MT6332_BIF_CON33, &qi_bif_rx_en, 0x1, 13);
        ret=pmic_read_interface(MT6332_BIF_CON33, &qi_bif_rx_data, 0x1, 12);
        ret=pmic_read_interface(MT6332_BIF_CON33, &bif_tx_state, 0x3, 10);
        ret=pmic_read_interface(MT6332_BIF_CON33, &bif_flow_ctl_state, 0x3, 8);        
        ret=pmic_read_interface(MT6332_BIF_CON33, &bif_rx_state, 0x7, 5);
        ret=pmic_read_interface(MT6332_BIF_CON19, &bif_data_num, 0xF, 0);
        ret=pmic_read_interface(MT6332_STA_CON1,  &rgs_baton_ht, 0x1, 10);

        printk("[bif_debug] %d,%d,%d,%d,%d,%d,%d,%d,%d\n", 
            qi_bif_tx_en, qi_bif_tx_data, qi_bif_rx_en, qi_bif_rx_data, bif_tx_state, bif_flow_ctl_state, bif_rx_state, bif_data_num, rgs_baton_ht);        
    }    
}

void tc_bif_reset_slave(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[1]={0};
    int loop_i=0;
    
    //1. set command sequence    
    printk("[tc_bif_reset_slave] 1. set command sequence\n");
    bif_cmd[0]=0x0400;
    set_bif_cmd(bif_cmd,1);

    //2. parameter setting
    printk("[tc_bif_reset_slave] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x0, 0x3, 8);
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_reset_slave] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0); 

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_reset_slave] 4. polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //disable BIF module
    printk("[tc_bif_reset_slave] 5. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0); 

    check_bat_lost();

    //reset BIF_IRQ
    reset_bif_irq();
}

void debug_dump_reg(void)
{
    U32 ret=0;
    U32 reg_val=0;
    
    ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0xFFFF, 0);
    printk("[debug_dump_reg] Reg[0x%x]=0x%x\n", MT6332_BIF_CON19, reg_val);

    ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0xFFFF, 0);
    printk("[debug_dump_reg] Reg[0x%x]=0x%x\n", MT6332_BIF_CON20, reg_val);

    ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0xFFFF, 0);
    printk("[debug_dump_reg] Reg[0x%x]=0x%x\n", MT6332_BIF_CON21, reg_val);

    ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0xFFFF, 0);
    printk("[debug_dump_reg] Reg[0x%x]=0x%x\n", MT6332_BIF_CON22, reg_val);

    ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0xFFFF, 0);
    printk("[debug_dump_reg] Reg[0x%x]=0x%x\n", MT6332_BIF_CON23, reg_val);
}

#if 1 // tc_bif_1000
void tc_bif_1000_step_0(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int loop_i=0;
    
    printk("[tc_bif_1000_step_0]-----------------------------------\n");

    printk("[tc_bif_1000_step_0] 1. set power up regiser\n");
    ret=pmic_config_interface(MT6332_BIF_CON32, 0x1, 0x1, 15);

    printk("[tc_bif_1000_step_0] 2. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);

    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON32,upmu_get_reg_value(MT6332_BIF_CON32),
        MT6332_BIF_CON18,upmu_get_reg_value(MT6332_BIF_CON18)
        );

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1000_step_0] 3. polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    printk("[tc_bif_1000_step_0] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);

    printk("[tc_bif_1000_step_0] 5. to disable power up mode\n");
    ret=pmic_config_interface(MT6332_BIF_CON32, 0x0, 0x1, 15);

    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON32,upmu_get_reg_value(MT6332_BIF_CON32),
        MT6332_BIF_CON18,upmu_get_reg_value(MT6332_BIF_CON18)
        );

    check_bat_lost();

    reset_bif_irq();
}

void tc_bif_1000_step_1(void)
{
    printk("[tc_bif_1000_step_1]-----------------------------------\n");

    tc_bif_reset_slave();    
}

void tc_bif_1000_step_2(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[3]={0,0,0};
    int loop_i=0;
    
    printk("[tc_bif_1000_step_2]-----------------------------------\n");

    //1. set command sequence    
    printk("[tc_bif_1000_step_2] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0100;
    bif_cmd[2]=0x0300;
    set_bif_cmd(bif_cmd,3);

    //2. parameter setting
    printk("[tc_bif_1000_step_2] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x3, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0x3, 8);    
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x1, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );

    //3. trigger BIF module
    printk("[tc_bif_1000_step_2] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1000_step_2] 4. polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //disable BIF module
    printk("[tc_bif_1000_step_2] 5. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    

    //data valid check
    printk("[tc_bif_1000_step_2] 6. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x7, 12);
    printk("[tc_bif_1000_step_2] BIF_CON31[14:12]=0x%x\n", reg_val);

    if(reg_val == 0)
    {
        //read the number of read back data package
        printk("[tc_bif_1000_step_2] 7. read the number of read back data package\n");
        ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0xF, 0);
        printk("[tc_bif_1000_step_2] BIF_DATA_NUM (1) : BIF_CON19[3:0]=0x%x\n", reg_val);        
        
        //read data back
        printk("[tc_bif_1000_step_2] 8. read data back\n");
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 15);
        printk("[tc_bif_1000_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 8);
        printk("[tc_bif_1000_step_2] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[tc_bif_1000_step_2] BIF_DATA_0 (10H) : BIF_CON20[7:0]=0x%x\n", reg_val);
    }
    else
    {
        printk("[tc_bif_1000_step_2] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);

        debug_dump_reg();
    }
 
    //reset BIF_IRQ
    reset_bif_irq();
}
#endif

#if 1 // tc_bif_1001
int pcap_15_8 = 0;
int pcap_7_0 = 0;
int pcreg_15_8 = 0;
int pcreg_7_0 = 0;

void tc_bif_1001_step_1(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;
    
    printk("[tc_bif_1001_step_1]-----------------------------------\n");
    
    //1. set command sequence    
    printk("[tc_bif_1001_step_1] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0424;
    bif_cmd[2]=0x0100;
    bif_cmd[3]=0x030A;
    set_bif_cmd(bif_cmd,4);
        
    //2. parameter setting
    printk("[tc_bif_1001_step_1] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x4, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0x3, 8);
    //ret=pmic_config_interface(MT6332_BIF_CON15, 0x7F, 0x7F, 0);
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x4, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1001_step_1] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1001_step_1] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //4. disable BIF module
    printk("[tc_bif_1001_step_1] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    //5. data valid check
    printk("[tc_bif_1001_step_1] 5. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x7, 12);
    printk("[tc_bif_1001_step_1] BIF_CON31[14:12]=0x%x\n", reg_val);    
    
    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[tc_bif_1001_step_1] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0xF, 0);
        printk("[tc_bif_1001_step_1] BIF_DATA_NUM (4) : BIF_CON19[3:0]=0x%x\n", reg_val);        
        
        //7. read data back
        printk("[tc_bif_1001_step_1] 7. read data back\n");
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 15);
        printk("[tc_bif_1001_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 8);
        printk("[tc_bif_1001_step_1] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[tc_bif_1001_step_1] BIF_DATA_0 (01H) : BIF_CON20[7:0]=0x%x\n", reg_val);
    
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 15);
        printk("[tc_bif_1001_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 8);
        printk("[tc_bif_1001_step_1] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[tc_bif_1001_step_1] BIF_DATA_0 (01H) : BIF_CON21[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0x1, 15);
        printk("[tc_bif_1001_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON22[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0x1, 8);
        printk("[tc_bif_1001_step_1] BIF_ACK_0 (1) : BIF_CON22[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0xFF, 0);
        printk("[tc_bif_1001_step_1] BIF_DATA_0 (pcap[15:8]=01H) : BIF_CON22[7:0]=0x%x\n", reg_val);
        pcap_15_8=reg_val;
    
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0x1, 15);
        printk("[tc_bif_1001_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON23[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0x1, 8);
        printk("[tc_bif_1001_step_1] BIF_ACK_0 (1) : BIF_CON23[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0xFF, 0);
        printk("[tc_bif_1001_step_1] BIF_DATA_0 (pcap[7:0]=00H) : BIF_CON23[7:0]=0x%x\n", reg_val);
        pcap_7_0=reg_val;
    }
    else
    {
        printk("[tc_bif_1001_step_1] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);

        debug_dump_reg();
    }
 
    //8. reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_1001_step_2(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;
    
    printk("[tc_bif_1001_step_2]-----------------------------------\n");
    
    //1. set command sequence    
    printk("[tc_bif_1001_step_2] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0422;
    bif_cmd[2]=(0x0100|pcap_15_8);
    bif_cmd[3]=(0x0300|pcap_7_0);
    set_bif_cmd(bif_cmd,4);
        
    //2. parameter setting
    printk("[tc_bif_1001_step_2] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x4, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0x3, 8);
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x2, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1001_step_2] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1001_step_2] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //4. disable BIF module
    printk("[tc_bif_1001_step_2] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    //5. data valid check
    printk("[tc_bif_1001_step_2] 5. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x7, 12);
    printk("[tc_bif_1001_step_2] BIF_CON31[14:12]=0x%x\n", reg_val);    
    
    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[tc_bif_1001_step_2] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0xF, 0);
        printk("[tc_bif_1001_step_2] BIF_DATA_NUM (2) : BIF_CON19[3:0]=0x%x\n", reg_val);        
        
        //7. read data back
        printk("[tc_bif_1001_step_2] 7. read data back\n");
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 15);
        printk("[tc_bif_1001_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 8);
        printk("[tc_bif_1001_step_2] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[tc_bif_1001_step_2] BIF_DATA_0 (0A) : BIF_CON20[7:0]=0x%x\n", reg_val);
        pcreg_15_8=reg_val;
    
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 15);
        printk("[tc_bif_1001_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 8);
        printk("[tc_bif_1001_step_2] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[tc_bif_1001_step_2] BIF_DATA_0 (00) : BIF_CON21[7:0]=0x%x\n", reg_val);
        pcreg_7_0=reg_val;
    }
    else
    {
        printk("[tc_bif_1001_step_2] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);

        debug_dump_reg();
    }
 
    //8. reset BIF_IRQ
    reset_bif_irq();
}


void tc_bif_1001_step_3(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;
    
    printk("[tc_bif_1001_step_3]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1001_step_3] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=(0x0100|pcreg_15_8);
    bif_cmd[2]=(0x0200|pcreg_7_0);
    bif_cmd[3]=0x0000;
    set_bif_cmd(bif_cmd,4);
        
    //2. parameter setting
    printk("[tc_bif_1001_step_3] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x4, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x0, 0x3, 8);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15)
        );
        
    //3. trigger BIF module
    printk("[tc_bif_1001_step_3] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1001_step_3] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //4. disable BIF module
    printk("[tc_bif_1001_step_3] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);

    check_bat_lost();
    
    //5. reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_1001_step_4(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[3]={0,0,0};
    int loop_i=0;
    
      printk("[tc_bif_1001_step_4]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1001_step_4] 1. set command sequence\n");
    bif_cmd[0]=0x0600;
    bif_cmd[1]=(0x0100|pcreg_15_8);
    bif_cmd[2]=(0x0300|pcreg_7_0);    
    set_bif_cmd(bif_cmd,3);       
        
    //2. parameter setting
    printk("[tc_bif_1001_step_4] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x3, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0x3, 8);    
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x1, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
        
    //3. trigger BIF module
    printk("[tc_bif_1001_step_4] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1001_step_4] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //4. disable BIF module
    printk("[tc_bif_1001_step_4] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    //5. data valid check
    printk("[tc_bif_1001_step_4] 5. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x7, 12);
    printk("[tc_bif_1001_step_4] BIF_CON31[14:12]=0x%x\n", reg_val);    
    
    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[tc_bif_1001_step_4] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0xF, 0);
        printk("[tc_bif_1001_step_4] BIF_DATA_NUM (1) : BIF_CON19[3:0]=0x%x\n", reg_val);        
        
        //7. read data back
        printk("[tc_bif_1001_step_4] 7. read data back\n");
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 15);
        printk("[tc_bif_1001_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 8);
        printk("[tc_bif_1001_step_4] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[tc_bif_1001_step_4] BIF_DATA_0 (00H) : BIF_CON20[7:0]=0x%x\n", reg_val);
    }
    else
    {
        printk("[tc_bif_1001_step_4] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);

        debug_dump_reg();
    }
 
    //8. reset BIF_IRQ
    reset_bif_irq(); 
}

void tc_bif_1001_step_5(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[5]={0,0,0,0,0};
    int loop_i=0;
    
      printk("[tc_bif_1001_step_5]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1001_step_5] 1. set command sequence\n");
    bif_cmd[0]=0x0600;
    bif_cmd[1]=(0x0100|pcreg_15_8);
    bif_cmd[2]=(0x0200|pcreg_7_0);
    bif_cmd[3]=0x0001;
    bif_cmd[4]=0x04C2;
    set_bif_cmd(bif_cmd,5);
        
    //2. parameter setting
    printk("[tc_bif_1001_step_5] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x5, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0x3, 8);    
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x1, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
        
    //3. trigger BIF module
    printk("[tc_bif_1001_step_5] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1001_step_5] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //4. disable BIF module
    printk("[tc_bif_1001_step_5] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    //5. data valid check
    printk("[tc_bif_1001_step_5] 5. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x7, 12);
    printk("[tc_bif_1001_step_5] BIF_CON31[14:12]=0x%x\n", reg_val); 
    
    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[tc_bif_1001_step_5] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0xF, 0);
        printk("[tc_bif_1001_step_5] BIF_DATA_NUM (1) : BIF_CON19[3:0]=0x%x\n", reg_val);        
        
        //7. read data back
        printk("[tc_bif_1001_step_5] 7. read data back\n");
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 15);
        printk("[tc_bif_1001_step_5] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 8);
        printk("[tc_bif_1001_step_5] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[tc_bif_1001_step_5] BIF_DATA_0 (04h) : BIF_CON20[7:0]=0x%x\n", reg_val);
    }
    else
    {
        printk("[tc_bif_1001_step_5] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);

        debug_dump_reg();
    }
    
    //8. reset BIF_IRQ
    reset_bif_irq();
}
#endif

#if 1 // tc_bif_1002
int nvmcap_15_8 = 0;
int nvmcap_7_0 = 0;
int nvmreg_15_8 = 0;
int nvmreg_7_0 = 0;

void tc_bif_1002_step_1(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;
    
      printk("[tc_bif_1002_step_1]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1002_step_1] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0424;
    bif_cmd[2]=0x0100;
    bif_cmd[3]=0x0316;    
    set_bif_cmd(bif_cmd,4);
        
    //2. parameter setting
    printk("[tc_bif_1002_step_1] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x4, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0x3, 8);
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x4, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1002_step_1] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1002_step_1] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //4. disable BIF module
    printk("[tc_bif_1002_step_1] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    //5. data valid check
    printk("[tc_bif_1002_step_1] 5. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x7, 12);
    printk("[tc_bif_1002_step_1] BIF_CON31[14:12]=0x%x\n", reg_val);    
    
    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[tc_bif_1002_step_1] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0xF, 0);
        printk("[tc_bif_1002_step_1] BIF_DATA_NUM (4) : BIF_CON19[3:0]=0x%x\n", reg_val);        
        
        //7. read data back
        printk("[tc_bif_1002_step_1] 7. read data back\n");
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 15);
        printk("[tc_bif_1002_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 8);
        printk("[tc_bif_1002_step_1] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[tc_bif_1002_step_1] BIF_DATA_0 (04H) : BIF_CON20[7:0]=0x%x\n", reg_val);
    
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 15);
        printk("[tc_bif_1002_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 8);
        printk("[tc_bif_1002_step_1] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[tc_bif_1002_step_1] BIF_DATA_0 (01H) : BIF_CON21[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0x1, 15);
        printk("[tc_bif_1002_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON22[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0x1, 8);
        printk("[tc_bif_1002_step_1] BIF_ACK_0 (1) : BIF_CON22[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0xFF, 0);
        printk("[tc_bif_1002_step_1] BIF_DATA_0 (pcap[15:8]=04H) : BIF_CON22[7:0]=0x%x\n", reg_val);
        nvmcap_15_8=reg_val;
    
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0x1, 15);
        printk("[tc_bif_1002_step_1] BIF_DATA_ERROR_0 (0) : BIF_CON23[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0x1, 8);
        printk("[tc_bif_1002_step_1] BIF_ACK_0 (1) : BIF_CON23[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0xFF, 0);
        printk("[tc_bif_1002_step_1] BIF_DATA_0 (pcap[7:0]=00H) : BIF_CON23[7:0]=0x%x\n", reg_val);
        nvmcap_7_0=reg_val;
    }
    else
    {
        printk("[tc_bif_1002_step_1] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }
 
    //8. reset BIF_IRQ
    reset_bif_irq(); 
}

void tc_bif_1002_step_2(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;
    
      printk("[tc_bif_1002_step_2]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1002_step_2] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0422;
    bif_cmd[2]=(0x0100|nvmcap_15_8);
    bif_cmd[3]=(0x0300|nvmcap_7_0);    
    set_bif_cmd(bif_cmd,4);
        
    //2. parameter setting
    printk("[tc_bif_1002_step_2] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x4, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0x3, 8);
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x2, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1002_step_2] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1002_step_2] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //4. disable BIF module
    printk("[tc_bif_1002_step_2] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    //5. data valid check
    printk("[tc_bif_1002_step_2] 5. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x7, 12);
    printk("[tc_bif_1002_step_2] BIF_CON31[14:12]=0x%x\n", reg_val);    
    
    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[tc_bif_1002_step_2] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0xF, 0);
        printk("[tc_bif_1002_step_2] BIF_DATA_NUM (2) : BIF_CON19[3:0]=0x%x\n", reg_val);        
        
        //7. read data back
        printk("[tc_bif_1002_step_2] 7. read data back\n");
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 15);
        printk("[tc_bif_1002_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 8);
        printk("[tc_bif_1002_step_2] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[tc_bif_1002_step_2] BIF_DATA_0 (nvmreg[15:8]=0FH) : BIF_CON20[7:0]=0x%x\n", reg_val);
        nvmreg_15_8=reg_val;
    
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 15);
        printk("[tc_bif_1002_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 8);
        printk("[tc_bif_1002_step_2] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[tc_bif_1002_step_2] BIF_DATA_0 (nvmreg[7:0]=00H) : BIF_CON21[7:0]=0x%x\n", reg_val);
        nvmreg_7_0=reg_val;
    }
    else
    {
        printk("[tc_bif_1002_step_2] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }
 
    //8. reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_1002_step_3(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[11]={0,0,0,0,0,0,0,0,0,0,0};
    int loop_i=0;
    
      printk("[tc_bif_1002_step_3]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1002_step_3] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=(0x0100|nvmreg_15_8);
    bif_cmd[2]=(0x0200|nvmcap_7_0) + 0x09;
    bif_cmd[3]=0x0000;
    bif_cmd[4]=0x0001;
    bif_cmd[5]=0x0002;
    bif_cmd[6]=0x0003;
    bif_cmd[7]=0x0004;
    bif_cmd[8]=0x0005;
    bif_cmd[9]=0x0006;
    bif_cmd[10]=0x04C2;
    set_bif_cmd(bif_cmd,11);
        
    //2. parameter setting
    printk("[tc_bif_1002_step_3] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0xB, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0x3, 8);
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x1, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1002_step_3] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1002_step_3] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //4. disable BIF module
    printk("[tc_bif_1002_step_3] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    //5. data valid check
    printk("[tc_bif_1002_step_3] 5. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x7, 12);
    printk("[tc_bif_1002_step_3] BIF_CON31[14:12]=0x%x\n", reg_val);    
    
    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[tc_bif_1002_step_3] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0xF, 0);
        printk("[tc_bif_1002_step_3] BIF_DATA_NUM (1) : BIF_CON19[3:0]=0x%x\n", reg_val);        
        
        //7. read data back
        printk("[tc_bif_1002_step_3] 7. read data back\n");
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 15);
        printk("[tc_bif_1002_step_3] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 8);
        printk("[tc_bif_1002_step_3] BIF_ACK_0 (0/1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[tc_bif_1002_step_3] BIF_DATA_0 (0Ah) : BIF_CON20[7:0]=0x%x\n", reg_val);        
    }
    else
    {
        printk("[tc_bif_1002_step_3] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }
 
    //8. reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_1002_step_4(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;
    
      printk("[tc_bif_1002_step_4]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1002_step_4] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0427;
    bif_cmd[2]=(0x0100|nvmreg_15_8);
    bif_cmd[3]=(0x0300|nvmcap_7_0) + 0x09;
    set_bif_cmd(bif_cmd,4);
        
    //2. parameter setting
    printk("[tc_bif_1002_step_4] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x4, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0x3, 8);
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x7, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1002_step_4] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1002_step_4] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //4. disable BIF module
    printk("[tc_bif_1002_step_4] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    //5. data valid check
    printk("[tc_bif_1002_step_4] 5. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x7, 12);
    printk("[tc_bif_1002_step_4] BIF_CON31[14:12]=0x%x\n", reg_val);    
    
    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[tc_bif_1002_step_4] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0xF, 0);
        printk("[tc_bif_1002_step_4] BIF_DATA_NUM (7) : BIF_CON19[3:0]=0x%x\n", reg_val);        
        
        //7. read data back
        printk("[tc_bif_1002_step_4] 7. read data back\n");
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 15);
        printk("[tc_bif_1002_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 8);
        printk("[tc_bif_1002_step_4] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[tc_bif_1002_step_4] BIF_DATA_0 (00h) : BIF_CON20[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 15);
        printk("[tc_bif_1002_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 8);
        printk("[tc_bif_1002_step_4] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[tc_bif_1002_step_4] BIF_DATA_0 (01h) : BIF_CON21[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0x1, 15);
        printk("[tc_bif_1002_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON22[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0x1, 8);
        printk("[tc_bif_1002_step_4] BIF_ACK_0 (1) : BIF_CON22[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0xFF, 0);
        printk("[tc_bif_1002_step_4] BIF_DATA_0 (02h) : BIF_CON22[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0x1, 15);
        printk("[tc_bif_1002_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON23[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0x1, 8);
        printk("[tc_bif_1002_step_4] BIF_ACK_0 (1) : BIF_CON23[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0xFF, 0);
        printk("[tc_bif_1002_step_4] BIF_DATA_0 (03h) : BIF_CON23[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON24, &reg_val, 0x1, 15);
        printk("[tc_bif_1002_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON24[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON24, &reg_val, 0x1, 8);
        printk("[tc_bif_1002_step_4] BIF_ACK_0 (1) : BIF_CON24[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON24, &reg_val, 0xFF, 0);
        printk("[tc_bif_1002_step_4] BIF_DATA_0 (04h) : BIF_CON24[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON25, &reg_val, 0x1, 15);
        printk("[tc_bif_1002_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON25[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON25, &reg_val, 0x1, 8);
        printk("[tc_bif_1002_step_4] BIF_ACK_0 (1) : BIF_CON25[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON25, &reg_val, 0xFF, 0);
        printk("[tc_bif_1002_step_4] BIF_DATA_0 (05h) : BIF_CON25[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON26, &reg_val, 0x1, 15);
        printk("[tc_bif_1002_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON26[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON26, &reg_val, 0x1, 8);
        printk("[tc_bif_1002_step_4] BIF_ACK_0 (1) : BIF_CON26[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON26, &reg_val, 0xFF, 0);
        printk("[tc_bif_1002_step_4] BIF_DATA_0 (06h) : BIF_CON26[7:0]=0x%x\n", reg_val);
    }
    else
    {
        printk("[tc_bif_1002_step_4] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }
 
    //8. reset BIF_IRQ
    reset_bif_irq(); 
}

#endif

#if 1 // tc_bif_1003
int sccap_15_8 = 0;
int sccap_7_0 = 0;
int screg_15_8 = 0;
int screg_7_0 = 0;

void tc_bif_1003_step_2(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;
    
      printk("[tc_bif_1003_step_2]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1003_step_2] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0424;
    bif_cmd[2]=0x0100;
    bif_cmd[3]=0x030E;
    set_bif_cmd(bif_cmd,4);
        
    //2. parameter setting
    printk("[tc_bif_1003_step_2] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x4, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0x3, 8);
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x4, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1003_step_2] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1003_step_2] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //4. disable BIF module
    printk("[tc_bif_1003_step_2] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    //5. data valid check
    printk("[tc_bif_1003_step_2] 5. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x7, 12);
    printk("[tc_bif_1003_step_2] BIF_CON31[14:12]=0x%x\n", reg_val);    
    
    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[tc_bif_1003_step_2] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0xF, 0);
        printk("[tc_bif_1003_step_2] BIF_DATA_NUM (4) : BIF_CON19[3:0]=0x%x\n", reg_val);        
        
        //7. read data back
        printk("[tc_bif_1003_step_2] 7. read data back\n");
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 15);
        printk("[tc_bif_1003_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 8);
        printk("[tc_bif_1003_step_2] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[tc_bif_1003_step_2] BIF_DATA_0 (02H) : BIF_CON20[7:0]=0x%x\n", reg_val);
    
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 15);
        printk("[tc_bif_1003_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 8);
        printk("[tc_bif_1003_step_2] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[tc_bif_1003_step_2] BIF_DATA_0 (01H) : BIF_CON21[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0x1, 15);
        printk("[tc_bif_1003_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON22[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0x1, 8);
        printk("[tc_bif_1003_step_2] BIF_ACK_0 (1) : BIF_CON22[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0xFF, 0);
        printk("[tc_bif_1003_step_2] BIF_DATA_0 (sccap[15:8]=02H) : BIF_CON22[7:0]=0x%x\n", reg_val);
        sccap_15_8=reg_val;
    
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0x1, 15);
        printk("[tc_bif_1003_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON23[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0x1, 8);
        printk("[tc_bif_1003_step_2] BIF_ACK_0 (1) : BIF_CON23[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0xFF, 0);
        printk("[tc_bif_1003_step_2] BIF_DATA_0 (sccap[7:0]=00H) : BIF_CON23[7:0]=0x%x\n", reg_val);
        sccap_7_0=reg_val;
    }
    else
    {
        printk("[tc_bif_1003_step_2] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }
 
    //8. reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_1003_step_3(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;
    
      printk("[tc_bif_1003_step_3]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1003_step_3] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0422;
    bif_cmd[2]=(0x0100|sccap_15_8);
    bif_cmd[3]=(0x0300|sccap_7_0);
    set_bif_cmd(bif_cmd,4);
        
    //2. parameter setting
    printk("[tc_bif_1003_step_3] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x4, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0x3, 8);
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x2, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1003_step_3] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1003_step_3] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //4. disable BIF module
    printk("[tc_bif_1003_step_3] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    //5. data valid check
    printk("[tc_bif_1003_step_3] 5. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x7, 12);
    printk("[tc_bif_1003_step_3] BIF_CON31[14:12]=0x%x\n", reg_val);    
    
    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[tc_bif_1003_step_3] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0xF, 0);
        printk("[tc_bif_1003_step_3] BIF_DATA_NUM (2) : BIF_CON19[3:0]=0x%x\n", reg_val);        
        
        //7. read data back
        printk("[tc_bif_1003_step_3] 7. read data back\n");
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 15);
        printk("[tc_bif_1003_step_3] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 8);
        printk("[tc_bif_1003_step_3] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[tc_bif_1003_step_3] BIF_DATA_0 (screg[15:8]=0CH) : BIF_CON20[7:0]=0x%x\n", reg_val);
        screg_15_8=reg_val;
    
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 15);
        printk("[tc_bif_1003_step_3] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 8);
        printk("[tc_bif_1003_step_3] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[tc_bif_1003_step_3] BIF_DATA_0 (screg[7:0]=00H) : BIF_CON21[7:0]=0x%x\n", reg_val);
        screg_7_0=reg_val;
    }
    else
    {
        printk("[tc_bif_1003_step_3] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }
 
    //8. reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_1003_step_4(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;
    
      printk("[tc_bif_1003_step_4]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1003_step_4] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=(0x0100|screg_15_8);
    bif_cmd[2]=(0x0200|screg_7_0);
    bif_cmd[3]=0x00FF;
    set_bif_cmd(bif_cmd,4);
        
    //2. parameter setting
    printk("[tc_bif_1003_step_4] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x4, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x0, 0x3, 8);
    //ret=pmic_config_interface(MT6332_BIF_CON17, 0x0, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1003_step_4] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1003_step_4] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //4. disable BIF module
    printk("[tc_bif_1003_step_4] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    

    check_bat_lost();
     
    //5. reset BIF_IRQ
    reset_bif_irq(); 
}

void tc_bif_1003_step_5(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[1]={0};
    int loop_i=0;
    
      printk("[tc_bif_1003_step_5]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1003_step_5] 1. set command sequence\n");
    bif_cmd[0]=0x0411;
    set_bif_cmd(bif_cmd,1);      
        
    //2. parameter setting
    printk("[tc_bif_1003_step_5] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x2, 0x3, 8);
        
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1003_step_5] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1003_step_5] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //4. disable BIF module
    printk("[tc_bif_1003_step_5] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    

    //5. data valid check
    printk("[tc_bif_1003_step_5] 5. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 13);
    printk("[tc_bif_1003_step_5] BIF_BAT_LOST : BIF_CON31[13]=0x%x - battery is detected(0)/battery is undetected(1)\n", reg_val);
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 12);
    printk("[tc_bif_1003_step_5] BIF_TIMEOUT : BIF_CON31[12]=0x%x - positive(0)/negative(1)\n", reg_val);
    ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0x1, 12);
    printk("[tc_bif_1003_step_5] BIF_RESPONSE : BIF_CON19[12]=0x%x - positive(1)/negative(0)\n", reg_val);
     
    //6. reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_1003_step_5_positive(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[1]={0};
    int loop_i=0;
    
    printk("[tc_bif_1003_step_5_positive]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1003_step_5_positive] 1. set command sequence\n");
    bif_cmd[0]=0x0411;
    set_bif_cmd(bif_cmd,1);      
        
    //2. parameter setting
    printk("[tc_bif_1003_step_5_positive] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x2, 0x3, 8);
    
    //new
    ret=pmic_config_interface(MT6332_BIF_CON15,0x7F, 0x7F,0);

    //only E2
    ret=pmic_config_interface(MT6332_BIF_CON37,0x1FF, 0x1FF,0);

    ret=pmic_config_interface(MT6332_BIF_CON30, 0x1, 0x1, 12);
    ret=pmic_config_interface(MT6332_BIF_CON30, 0x1, 0x1, 4);
        
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17),
        MT6332_BIF_CON30,upmu_get_reg_value(MT6332_BIF_CON30),
        MT6332_BIF_CON37,upmu_get_reg_value(MT6332_BIF_CON37)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1003_step_5_positive] 5. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();

    //new
    mdelay(10);
    ret=pmic_config_interface(MT6332_BIF_CON30, 0x0, 0x1, 12);
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "BIF_RX_DATA_SW=0 : Reg[0x%x]=0x%x\n",         
        MT6332_BIF_CON30,upmu_get_reg_value(MT6332_BIF_CON30)
        );
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1003_step_5_positive] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //disable BIF module
    printk("[tc_bif_1003_step_5_positive] disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0); 

    printk("[tc_bif_1003_step_5_positive] disable SW control\n");
    ret=pmic_config_interface(MT6332_BIF_CON30, 0x0, 0x1, 4);

    //data valid check
    printk("[tc_bif_1003_step_5_positive] 8. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 13);
    printk("[tc_bif_1003_step_5_positive] BIF_BAT_LOST : BIF_CON31[13]=0x%x - battery is detected(0)/battery is undetected(1)\n", reg_val);
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 12);
    printk("[tc_bif_1003_step_5_positive] BIF_TIMEOUT : BIF_CON31[12]=0x%x - positive(0)/negative(1)\n", reg_val);
    ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0x1, 12);
    printk("[tc_bif_1003_step_5_positive] BIF_RESPONSE : BIF_CON19[12]=0x%x - positive(1)/negative(0)\n", reg_val);
     
    //6. reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_1003_step_5_negative(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[1]={0};
    int loop_i=0;
    
      printk("[tc_bif_1003_step_5_negative]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1003_step_5_negative] 1. set command sequence\n");
    bif_cmd[0]=0x0411;
    set_bif_cmd(bif_cmd,1);      
        
    //2. parameter setting
    printk("[tc_bif_1003_step_5_negative] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x2, 0x3, 8);
        
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1003_step_5_negative] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1003_step_5_negative] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //4. disable BIF module
    printk("[tc_bif_1003_step_5_negative] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    

    //5. data valid check
    printk("[tc_bif_1003_step_5_negative] 5. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 13);
    printk("[tc_bif_1003_step_5_negative] BIF_BAT_LOST : BIF_CON31[13]=0x%x - battery is detected(0)/battery is undetected(1)\n", reg_val);
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 12);
    printk("[tc_bif_1003_step_5_negative] BIF_TIMEOUT : BIF_CON31[12]=0x%x - positive(0)/negative(1)\n", reg_val);
    ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0x1, 12);
    printk("[tc_bif_1003_step_5_negative] BIF_RESPONSE : BIF_CON19[12]=0x%x - positive(1)/negative(0)\n", reg_val);
     
    //6. reset BIF_IRQ
    reset_bif_irq();
}
#endif

#if 1 // tc_bif_1004
void tc_bif_1004_step_1_positive(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[1]={0};
    int loop_i=0;
    
      printk("[tc_bif_1004_step_1_positive]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1004_step_1_positive] 1. set command sequence\n");
    bif_cmd[0]=0x0410;    
    set_bif_cmd(bif_cmd,1);
        
    //2. parameter setting
    printk("[tc_bif_1004_step_1_positive] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x2, 0x3, 8);
        
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1004_step_1_positive] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    //----------------------------------------------------------------
    udelay(10);
    bif_debug();
    mdelay(1000);
    printk("[tc_bif_1004_step_1_positive] wait 1s and then\n");

    //new
    ret=pmic_config_interface(MT6332_BIF_CON30, 0x0, 0x1, 12);
    ret=pmic_config_interface(MT6332_BIF_CON30, 0x1, 0x1, 4);
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "after set, Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON30,upmu_get_reg_value(MT6332_BIF_CON30)
        );
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1004_step_1_positive] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //5. disable BIF module
    printk("[tc_bif_1004_step_1_positive] 5. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);   

    printk("[tc_bif_1004_step_1_positive] disable SW control\n");
    ret=pmic_config_interface(MT6332_BIF_CON30, 0x0, 0x1, 4);
    
    //6. data valid check
    printk("[tc_bif_1004_step_1_positive] 6. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 13);
    printk("[tc_bif_1004_step_1_positive] BIF_BAT_LOST : BIF_CON31[13]=0x%x - battery is detected(0)/battery is undetected(1)\n", reg_val);
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 12);
    printk("[tc_bif_1004_step_1_positive] BIF_TIMEOUT : BIF_CON31[12]=0x%x - positive(0)/negative(1)\n", reg_val);
    ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0x1, 12);
    printk("[tc_bif_1004_step_1_positive] BIF_RESPONSE : BIF_CON19[12]=0x%x - positive(1)/negative(0)\n", reg_val);
     
    //6. reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_1004_step_1_negative(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[1]={0};
    int loop_i=0;
    
      printk("[tc_bif_1004_step_1_negative]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1004_step_1_negative] 1. set command sequence\n");
    bif_cmd[0]=0x0410;    
    set_bif_cmd(bif_cmd,1);
        
    //2. parameter setting
    printk("[tc_bif_1004_step_1_negative] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x2, 0x3, 8);
        
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1004_step_1_negative] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    //----------------------------------------------------------------
    udelay(10);
    bif_debug();
    mdelay(1000);
    printk("[tc_bif_1004_step_1_negative] wait 1s and then\n");

    //4. BIF_BACK_NORMAL = 1
    printk("[tc_bif_1004_step_1_negative] 4. BIF_BACK_NORMAL = 1\n");
    ret=pmic_config_interface(MT6332_BIF_CON31, 0x1, 0x1, 0);
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON31,upmu_get_reg_value(MT6332_BIF_CON31)
        );
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1004_step_1_negative] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }
    
    //5. disable BIF module
    printk("[tc_bif_1004_step_1_negative] 5. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    printk("[tc_bif_1004_step_1_negative] clear rg_back_normal\n");
    ret=pmic_config_interface(MT6332_BIF_CON31, 0x0, 0x1, 0);
 
    //6. data valid check
    printk("[tc_bif_1004_step_1_negative] 6. data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 13);
    printk("[tc_bif_1004_step_1_negative] BIF_BAT_LOST : BIF_CON31[13]=0x%x - battery is detected(0)/battery is undetected(1)\n", reg_val);
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 12);
    printk("[tc_bif_1004_step_1_negative] BIF_TIMEOUT : BIF_CON31[12]=0x%x - positive(0)/negative(1)\n", reg_val);
    ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0x1, 12);
    printk("[tc_bif_1004_step_1_negative] BIF_RESPONSE : BIF_CON19[12]=0x%x - positive(1)/negative(0)\n", reg_val);
     
    //6. reset BIF_IRQ
    reset_bif_irq();
}

#endif

#if 1 // tc_bif_1005
void tc_bif_1005_step1(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[11]={0,0,0,0,0,0,0,0,0,0,0};
    
    printk("[tc_bif_1005_step1]-----------------------------------\n");

    printk("[tc_bif_1005_step1] 1. sw control and switch to sw mode\n");
    ret=pmic_config_interface(MT6332_BIF_CON30, 0x1, 0x1, 11);
    ret=pmic_config_interface(MT6332_BIF_CON30, 0x1, 0x1, 6);
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON30,upmu_get_reg_value(MT6332_BIF_CON30)
        );
    
    //set command sequence
    printk("[tc_bif_1005_step1] 2. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x010F;
    bif_cmd[2]=0x0209;
    bif_cmd[3]=0x0000;
    bif_cmd[4]=0x0001;
    bif_cmd[5]=0x0002;
    bif_cmd[6]=0x0003;
    bif_cmd[7]=0x0004;
    bif_cmd[8]=0x0005;
    bif_cmd[9]=0x0006;
    bif_cmd[10]=0x04C2;
    set_bif_cmd(bif_cmd,11);
        
    //parameter setting
    printk("[tc_bif_1005_step1] 3. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0xB, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x0, 0x3, 8);
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x1, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1005_step1] 4. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    mdelay(1000);

    printk("[tc_bif_1005_step1] After 1s\n");
    
    reg_val=0;    
    //while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1005_step1] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);
        printk("[tc_bif_1005_step1] if bif_irq == 0, pass\n");
    }
    
    //disable BIF module
    printk("[tc_bif_1005_step1] disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    

    //reset BIF_IRQ
    reset_bif_irq();
}
#endif

#if 1 // tc_bif_1006
void tc_bif_1006_step_1(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[11]={0,0,0,0,0,0,0,0,0,0,0};
    int loop_i=0;
    
    printk("[tc_bif_1006_step_1]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1006_step_1] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x010F;
    bif_cmd[2]=0x0209;
    bif_cmd[3]=0x0000;
    bif_cmd[4]=0x0001;
    bif_cmd[5]=0x0002;
    bif_cmd[6]=0x0003;
    bif_cmd[7]=0x0004;
    bif_cmd[8]=0x0005;
    bif_cmd[9]=0x0006;
    bif_cmd[10]=0x04C2;
    set_bif_cmd(bif_cmd,11);
        
    //2. parameter setting
    printk("[tc_bif_1006_step_1] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0xB, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x0, 0x3, 8);
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x1, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1006_step_1] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    mdelay(10);
    printk("[tc_bif_1006_step_1] wait 10ms and then\n");

    ret=pmic_config_interface(MT6332_BIF_CON30, 0x1, 0x1, 11);
    ret=pmic_config_interface(MT6332_BIF_CON30, 0x1, 0x1, 6);    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON30,upmu_get_reg_value(MT6332_BIF_CON30)
        );
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1006_step_1] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }

    printk("[tc_bif_1006_step_1] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

    ret=pmic_config_interface(MT6332_BIF_CON30, 0x0, 0x1, 6);
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "disable sw control : Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON30,upmu_get_reg_value(MT6332_BIF_CON30)
        );
    
    //disable BIF module
    printk("[tc_bif_1006_step_1] disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    //data valid check
    printk("[tc_bif_1006_step_1] data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 12);
    printk("[tc_bif_1006_step_1] BIF_CON31[12]=0x%x, need 0\n", reg_val);    
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 13);
    printk("[tc_bif_1006_step_1] BIF_CON31[13]=0x%x, need 1\n", reg_val);
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 14);
    printk("[tc_bif_1006_step_1] BIF_CON31[14]=0x%x, need 0\n", reg_val);
 
    //reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_1006_step_2(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[11]={0,0,0,0,0,0,0,0,0,0,0};
    int loop_i=0;
    
    printk("[tc_bif_1006_step_2]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1006_step_2] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x010F;
    bif_cmd[2]=0x0209;
    bif_cmd[3]=0x0000;
    bif_cmd[4]=0x0001;
    bif_cmd[5]=0x0002;
    bif_cmd[6]=0x0003;
    bif_cmd[7]=0x0004;
    bif_cmd[8]=0x0005;
    bif_cmd[9]=0x0006;
    bif_cmd[10]=0x04C2;
    set_bif_cmd(bif_cmd,11);
        
    //2. parameter setting
    printk("[tc_bif_1006_step_2] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0xB, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x0, 0x3, 8);
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x1, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1006_step_2] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1006_step_2] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }

    printk("[tc_bif_1006_step_2] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);
    
    //disable BIF module
    printk("[tc_bif_1006_step_2] disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    //data valid check
    printk("[tc_bif_1006_step_2] data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x7, 12);
    printk("[tc_bif_1006_step_2] BIF_CON31[14:12]=0x%x\n", reg_val);    
    
    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[tc_bif_1006_step_2] read the number of read back data package\n");
        ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0xF, 0);
        printk("[tc_bif_1006_step_2] BIF_DATA_NUM (1) : BIF_CON19[3:0]=0x%x\n", reg_val);        
        
        //7. read data back
        printk("[tc_bif_1006_step_2] read data back\n");
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 15);
        printk("[tc_bif_1006_step_2] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 8);
        printk("[tc_bif_1006_step_2] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[tc_bif_1006_step_2] BIF_DATA_0 (0AH) : BIF_CON20[7:0]=0x%x\n", reg_val);
    }
    else
    {
        printk("[tc_bif_1006_step_2] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }
 
    //reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_1006_step_3(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;
    
    printk("[tc_bif_1006_step_3]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1006_step_3] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0427;
    bif_cmd[2]=0x010F;
    bif_cmd[3]=0x0309;
    set_bif_cmd(bif_cmd,4);
        
    //2. parameter setting
    printk("[tc_bif_1006_step_3] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x4, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0x3, 8);
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x7, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1006_step_3] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
    
    mdelay(10);
    printk("[tc_bif_1006_step_3] wait 10ms and then\n");

    ret=pmic_config_interface(MT6332_BIF_CON30, 0x1, 0x1, 11);
    ret=pmic_config_interface(MT6332_BIF_CON30, 0x1, 0x1, 6);    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON30,upmu_get_reg_value(MT6332_BIF_CON30)
        );
    
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1006_step_3] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }

    printk("[tc_bif_1006_step_3] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

    ret=pmic_config_interface(MT6332_BIF_CON30, 0x0, 0x1, 6);
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "disable sw control : Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON30,upmu_get_reg_value(MT6332_BIF_CON30)
        );
    
    //disable BIF module
    printk("[tc_bif_1006_step_3] disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    //data valid check
    printk("[tc_bif_1006_step_3] data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 12);
    printk("[tc_bif_1006_step_3] BIF_CON31[12]=0x%x, need 0\n", reg_val);    
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 13);
    printk("[tc_bif_1006_step_3] BIF_CON31[13]=0x%x, need 1\n", reg_val);
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 14);
    printk("[tc_bif_1006_step_3] BIF_CON31[14]=0x%x, need 0\n", reg_val);
 
    //reset BIF_IRQ
    reset_bif_irq();
}

void tc_bif_1006_step_4(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int bif_cmd[4]={0,0,0,0};
    int loop_i=0;
    
    printk("[tc_bif_1006_step_4]-----------------------------------\n");
    
    //1. set command sequence
    printk("[tc_bif_1006_step_4] 1. set command sequence\n");
    bif_cmd[0]=0x0601;
    bif_cmd[1]=0x0427;
    bif_cmd[2]=0x010F;
    bif_cmd[3]=0x0309;
    set_bif_cmd(bif_cmd,4);
        
    //2. parameter setting
    printk("[tc_bif_1006_step_4] 2. parameter setting\n");
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x4, 0xF, 12);
    ret=pmic_config_interface(MT6332_BIF_CON15, 0x1, 0x3, 8);
    ret=pmic_config_interface(MT6332_BIF_CON17, 0x7, 0xF, 12);    
    
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON15,upmu_get_reg_value(MT6332_BIF_CON15),
        MT6332_BIF_CON17,upmu_get_reg_value(MT6332_BIF_CON17)
        );
    
    //3. trigger BIF module
    printk("[tc_bif_1006_step_4] 3. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);    

    udelay(10);
    bif_debug();
        
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1006_step_4] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

        if(loop_i++ > g_loop_out) break;
    }

    printk("[tc_bif_1006_step_4] polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);

    //disable BIF module
    printk("[tc_bif_1006_step_4] disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);    
    
    //data valid check
    printk("[tc_bif_1006_step_4] data valid check\n");
    ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x7, 12);
    printk("[tc_bif_1006_step_4] BIF_CON31[14:12]=0x%x\n", reg_val);    
    
    if(reg_val == 0)
    {
        //6. read the number of read back data package
        printk("[tc_bif_1006_step_4] 6. read the number of read back data package\n");
        ret=pmic_read_interface(MT6332_BIF_CON19, &reg_val, 0xF, 0);
        printk("[tc_bif_1006_step_4] BIF_DATA_NUM (7) : BIF_CON19[3:0]=0x%x\n", reg_val);        
        
        //7. read data back
        printk("[tc_bif_1006_step_4] 7. read data back\n");
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 15);
        printk("[tc_bif_1006_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON20[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0x1, 8);
        printk("[tc_bif_1006_step_4] BIF_ACK_0 (1) : BIF_CON20[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON20, &reg_val, 0xFF, 0);
        printk("[tc_bif_1006_step_4] BIF_DATA_0 (00h) : BIF_CON20[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 15);
        printk("[tc_bif_1006_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON21[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0x1, 8);
        printk("[tc_bif_1006_step_4] BIF_ACK_0 (1) : BIF_CON21[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON21, &reg_val, 0xFF, 0);
        printk("[tc_bif_1006_step_4] BIF_DATA_0 (01h) : BIF_CON21[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0x1, 15);
        printk("[tc_bif_1006_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON22[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0x1, 8);
        printk("[tc_bif_1006_step_4] BIF_ACK_0 (1) : BIF_CON22[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON22, &reg_val, 0xFF, 0);
        printk("[tc_bif_1006_step_4] BIF_DATA_0 (02h) : BIF_CON22[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0x1, 15);
        printk("[tc_bif_1006_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON23[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0x1, 8);
        printk("[tc_bif_1006_step_4] BIF_ACK_0 (1) : BIF_CON23[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON23, &reg_val, 0xFF, 0);
        printk("[tc_bif_1006_step_4] BIF_DATA_0 (03h) : BIF_CON23[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON24, &reg_val, 0x1, 15);
        printk("[tc_bif_1006_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON24[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON24, &reg_val, 0x1, 8);
        printk("[tc_bif_1006_step_4] BIF_ACK_0 (1) : BIF_CON24[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON24, &reg_val, 0xFF, 0);
        printk("[tc_bif_1006_step_4] BIF_DATA_0 (04h) : BIF_CON24[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON25, &reg_val, 0x1, 15);
        printk("[tc_bif_1006_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON25[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON25, &reg_val, 0x1, 8);
        printk("[tc_bif_1006_step_4] BIF_ACK_0 (1) : BIF_CON25[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON25, &reg_val, 0xFF, 0);
        printk("[tc_bif_1006_step_4] BIF_DATA_0 (05h) : BIF_CON25[7:0]=0x%x\n", reg_val);
        
        ret=pmic_read_interface(MT6332_BIF_CON26, &reg_val, 0x1, 15);
        printk("[tc_bif_1006_step_4] BIF_DATA_ERROR_0 (0) : BIF_CON26[15]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON26, &reg_val, 0x1, 8);
        printk("[tc_bif_1006_step_4] BIF_ACK_0 (1) : BIF_CON26[8]=0x%x\n", reg_val);
        ret=pmic_read_interface(MT6332_BIF_CON26, &reg_val, 0xFF, 0);
        printk("[tc_bif_1006_step_4] BIF_DATA_0 (06h) : BIF_CON26[7:0]=0x%x\n", reg_val);
    }
    else
    {
        printk("[tc_bif_1006_step_4] Fail : BIF_CON31[14:12]=0x%x\n", reg_val);
    }
 
    //reset BIF_IRQ
    reset_bif_irq();
}
#endif

#if 1
void tc_bif_1008_step_0(void)
{
    U32 ret=0;

    
    printk("[tc_bif_1008_step_0]-----------------------------------\n");

    printk("[tc_bif_1008_step_0] 1. set power up regiser\n");
    ret=pmic_config_interface(MT6332_BIF_CON32, 0x1, 0x1, 15);

    printk("[tc_bif_1008_step_0] 2. trigger BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x1, 0x1, 0);

    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON32,upmu_get_reg_value(MT6332_BIF_CON32),
        MT6332_BIF_CON18,upmu_get_reg_value(MT6332_BIF_CON18)
        );

    udelay(10);
    bif_debug();

    #if 0
    reg_val=0;    
    while(reg_val == 0)
    {
        ret=pmic_read_interface(MT6332_BIF_CON31, &reg_val, 0x1, 11);
        printk("[tc_bif_1008_step_0] 3. polling BIF_IRQ : BIF_CON31[11]=0x%x\n", reg_val);
    }
    #else
    printk("[tc_bif_1008_step_0] wait EINT\n");   
    #endif    
}

void tc_bif_1008_step_1(void)
{
    U32 ret=0;

    
    printk("[tc_bif_1008_step_1]-----------------------------------\n");
    
    printk("[tc_bif_1008_step_1] 4. disable BIF module\n");
    ret=pmic_config_interface(MT6332_BIF_CON18, 0x0, 0x1, 0);

    printk("[tc_bif_1008_step_1] 5. to disable power up mode\n");
    ret=pmic_config_interface(MT6332_BIF_CON32, 0x0, 0x1, 15);

    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        MT6332_BIF_CON32,upmu_get_reg_value(MT6332_BIF_CON32),
        MT6332_BIF_CON18,upmu_get_reg_value(MT6332_BIF_CON18)
        );

    check_bat_lost();

    reset_bif_irq();
}

#endif

void bif_init(void)
{
    //On PMU EVB need :
    //cd /sys/devices/platform/mt-pmic/
    //echo 001E 0000 > pmic_access
    //echo 000E FFFF > pmic_access
    //echo 8C20 0000 > pmic_access
    //echo 8C12 FFFF > pmic_access

    //enable BIF interrupt
    mt6332_upmu_set_rg_int_en_bif(1);

    //enable BIF clock
    upmu_set_reg_value(MT6332_TOP_CKPDN_CON0_CLR, 0x80C0);

    //enable HT protection
    //pmic_config_interface(MT6332_BATON_CON0, 0xF, 0xF, 0);

    //turn on LDO
    mt6332_upmu_set_rg_vbif28_en(1);

    printk("[bif_init] done. Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
        MT6332_INT_CON2, upmu_get_reg_value(MT6332_INT_CON2),
        MT6332_TOP_CKPDN_CON0, upmu_get_reg_value(MT6332_TOP_CKPDN_CON0),
        MT6332_BATON_CON0, upmu_get_reg_value(MT6332_BATON_CON0),
        MT6332_LDO_CON2, upmu_get_reg_value(MT6332_LDO_CON2)
        );
}

void tc_bif_1000(void)
{
    printk("[tc_bif_1000] run read/write function\n");

    bif_init();

    tc_bif_1000_step_0();
    mdelay(10);
    tc_bif_1000_step_1();    
    tc_bif_1000_step_2();
}

void tc_bif_1001(void)
{
    printk("[tc_bif_1001] run read/write function\n");

    tc_bif_reset_slave();
    mdelay(1);
    
    tc_bif_1001_step_1();//read 4 data
    mdelay(500);
    tc_bif_1001_step_1();//read 4 data
    mdelay(500);
    tc_bif_1001_step_1();//read 4 data
    mdelay(500);
    tc_bif_1001_step_1();//read 4 data
    mdelay(500);
    tc_bif_1001_step_1();//read 4 data
    mdelay(500);
    
    tc_bif_1001_step_2();//read 2 data
    mdelay(500);
    tc_bif_1001_step_2();//read 2 data
    mdelay(500);
    tc_bif_1001_step_2();//read 2 data
    mdelay(500);
    tc_bif_1001_step_2();//read 2 data
    mdelay(500);
    tc_bif_1001_step_2();//read 2 data
    mdelay(500);
    
    tc_bif_1001_step_3();//write
    tc_bif_1001_step_4();//check write is ok or not    
    tc_bif_1001_step_5();//change back to the original device address with TQ    
}

void tc_bif_1002(void)
{
    printk("[tc_bif_1002] run burst write/read function\n");

    tc_bif_reset_slave();
    mdelay(1);
    
    tc_bif_1002_step_1(); //burst read 4 data
    mdelay(500);
    tc_bif_1002_step_1(); //burst read 4 data
    mdelay(500);
    tc_bif_1002_step_1(); //burst read 4 data
    mdelay(500);
    tc_bif_1002_step_1(); //burst read 4 data
    mdelay(500);
    tc_bif_1002_step_1(); //burst read 4 data
    mdelay(500);
    
    tc_bif_1002_step_2(); //burst read 2 data
    mdelay(500);
    tc_bif_1002_step_2(); //burst read 2 data
    mdelay(500);
    tc_bif_1002_step_2(); //burst read 2 data
    mdelay(500);
    tc_bif_1002_step_2(); //burst read 2 data
    mdelay(500);
    tc_bif_1002_step_2(); //burst read 2 data
    mdelay(500);
    
    tc_bif_1002_step_3(); //burst write 8 date
    
    tc_bif_1002_step_4(); //burst read 8 date
    mdelay(500);
    tc_bif_1002_step_4(); //burst read 8 date
    mdelay(500);
    tc_bif_1002_step_4(); //burst read 8 date
    mdelay(500);
    tc_bif_1002_step_4(); //burst read 8 date
    mdelay(500);
    tc_bif_1002_step_4(); //burst read 8 date
    mdelay(500);
}

void tc_bif_1003(void)
{
    printk("[tc_bif_1003] run bus command (ISTS)\n");

    pmic_config_interface(MT6332_TOP_CKSEL_CON0, 0x4, MT6332_PMIC_RG_BIF_X4_CK_DIVSEL_MASK, MT6332_PMIC_RG_BIF_X4_CK_DIVSEL_SHIFT);
    printk("[RG_BIF_X4_CK_DIVSEL] set done. Reg[0x%x]=0x%x\n",
        MT6332_TOP_CKSEL_CON0, upmu_get_reg_value(MT6332_TOP_CKSEL_CON0)
        );

    pmic_config_interface(MT6332_BIF_CON15,0x7F,0x7F, 0);    
    printk("[RG_BIF_X4_CK_DIVSEL] Reg[0x%x]=0x%x\n",
            MT6332_BIF_CON15, upmu_get_reg_value(MT6332_BIF_CON15)
            );

    tc_bif_reset_slave();
    mdelay(1);
    mdelay(500);
    
    tc_bif_1003_step_2(); //burst read 4 data
    mdelay(500);
    tc_bif_1003_step_2(); //burst read 4 data
    mdelay(500);
    tc_bif_1003_step_2(); //burst read 4 data
    mdelay(500);
    tc_bif_1003_step_2(); //burst read 4 data
    mdelay(500);
    tc_bif_1003_step_2(); //burst read 4 data
    mdelay(500);
    
    tc_bif_1003_step_3(); //burst read 2 data
    mdelay(500);
    tc_bif_1003_step_3(); //burst read 2 data
    mdelay(500);
    tc_bif_1003_step_3(); //burst read 2 data
    mdelay(500);
    tc_bif_1003_step_3(); //burst read 2 data
    mdelay(500);
    tc_bif_1003_step_3(); //burst read 2 data
    mdelay(500);
    
    tc_bif_1003_step_4(); //write enable irq
    
    //tc_bif_1003_step_5(); //write bus command ISTS
    mdelay(2000);    
    tc_bif_1003_step_5_negative(); //write bus command ISTS : negative response
    mdelay(1000);    
    tc_bif_1003_step_5_positive(); //write bus command ISTS : positive response    
}

void tc_bif_1004(void)
{
    printk("[tc_bif_1004] run interrupt mode (EINT)\n");
    
    tc_bif_1004_step_1_positive();
    tc_bif_1004_step_1_negative();
}

void tc_bif_1005(void)
{
    printk("[tc_bif_1005] battery lost\n");

    tc_bif_1005_step1();
}

void tc_bif_1006(void)
{
    printk("[tc_bif_1006] battery lost test : E1 option\n");

    tc_bif_reset_slave();
    mdelay(1);
    tc_bif_1006_step_1();
    tc_bif_1006_step_2();
    tc_bif_1006_step_3();
    tc_bif_1006_step_4();    
}

void tc_bif_1007(void)
{
    printk("[tc_bif_1007] BIF EINT test\n");

    bif_init();

    tc_bif_1000_step_0();
}

void tc_bif_1008(void)
{
    printk("[tc_bif_1008] BIF EINT test\n");

    bif_init();

    tc_bif_1008_step_0();
}

#endif

#ifdef TOP_DVT_ENABLE
static kal_uint32 mt6331_aPMURegDef[569][2]= {  /* Register , reset val*/
  {0x0000, 0x0040}, /*STRUP_CON0*/
  {0x0002, 0x0000}, /*STRUP_CON2*/
  {0x0004, 0x4001}, /*STRUP_CON3*/
  {0x0006, 0x0000}, /*STRUP_CON4*/
  {0x0008, 0x0000}, /*STRUP_CON5*/
  {0x000A, 0x0000}, /*STRUP_CON6*/
  {0x000C, 0x0000}, /*STRUP_CON7*/
  {0x000E, 0x0000}, /*STRUP_CON8*/
  {0x0010, 0x0000}, /*STRUP_CON9*/
  {0x0012, 0x0000}, /*STRUP_CON10*/
  {0x0014, 0x0020}, /*STRUP_CON11*/
  {0x0016, 0x0000}, /*STRUP_CON12*/
  {0x0018, 0x5FC0}, /*STRUP_CON13*/
  {0x001A, 0x0000}, /*STRUP_CON14*/
  {0x001C, 0x0000}, /*STRUP_CON15*/
  {0x001E, 0x0800}, /*STRUP_CON16*/
  {0x0020, 0x0000}, /*STRUP_CON17*/
  {0x0022, 0x0000}, /*STRUP_CON18*/
  {0x0100, 0x3110}, /*HWCID*/
  {0x0102, 0x3110}, /*SWCID*/
  {0x0104, 0x0000}, /*EXT_PMIC_STATUS*/
  {0x0106, 0x0103}, /*TOP_CON*/
  {0x0108, 0x0000}, /*TEST_OUT*/
  {0x010A, 0x0000}, /*TEST_CON0*/
  {0x010C, 0x0000}, /*TEST_CON1*/
  {0x010E, 0x0000}, /*TESTMODE_SW*/
  {0x0110, 0x0000}, /*EN_STATUS0*/
  {0x0112, 0x0000}, /*EN_STATUS1*/
  {0x0114, 0x0000}, /*EN_STATUS2*/
  {0x0116, 0x0000}, /*OCSTATUS0*/
  {0x0118, 0x0000}, /*OCSTATUS1*/
  {0x011A, 0x0000}, /*OCSTATUS2*/
  {0x011C, 0x0000}, /*PGSTATUS*/
  {0x011E, 0x0000}, /*TOPSTATUS*/
  {0x0120, 0x0000}, /*TDSEL_CON*/
  {0x0122, 0x0000}, /*RDSEL_CON*/
  {0x0124, 0x003C}, /*SMT_CON0*/
  {0x0126, 0x0000}, /*SMT_CON1*/
  {0x0128, 0x0000}, /*SMT_CON2*/
  {0x012A, 0xCCCC}, /*DRV_CON0*/
  {0x012C, 0xCCCC}, /*DRV_CON1*/
  {0x012E, 0xCCCC}, /*DRV_CON2*/
  {0x0130, 0x00CC}, /*DRV_CON3*/
  {0x0132, 0x0000}, /*TOP_STATUS*/
  {0x0134, 0x0000}, /*TOP_STATUS_SET*/
  {0x0136, 0x0000}, /*TOP_STATUS_CLR*/
  {0x0138, 0xEEFE}, /*TOP_CKPDN_CON0*/
  {0x013A, 0x0000}, /*TOP_CKPDN_CON0_SET*/
  {0x013C, 0x0000}, /*TOP_CKPDN_CON0_CLR*/
  {0x013E, 0x0AA0}, /*TOP_CKPDN_CON1*/
  {0x0140, 0x0000}, /*TOP_CKPDN_CON1_SET*/
  {0x0142, 0x0000}, /*TOP_CKPDN_CON1_CLR*/
  {0x0144, 0x0008}, /*TOP_CKPDN_CON2*/
  {0x0146, 0x0000}, /*TOP_CKPDN_CON2_SET*/
  {0x0148, 0x0000}, /*TOP_CKPDN_CON2_CLR*/
  {0x014A, 0x1000}, /*TOP_CKSEL_CON*/
  {0x014C, 0x0000}, /*TOP_CKSEL_CON_SET*/
  {0x014E, 0x0000}, /*TOP_CKSEL_CON_CLR*/
  {0x0150, 0x00FF}, /*TOP_CKHWEN_CON*/
  {0x0152, 0x0000}, /*TOP_CKHWEN_CON_SET*/
  {0x0154, 0x0000}, /*TOP_CKHWEN_CON_CLR*/
  {0x0156, 0x0000}, /*TOP_CKTST_CON0*/
  {0x0158, 0x0000}, /*TOP_CKTST_CON1*/
  {0x015A, 0x0000}, /*TOP_CLKSQ*/
  {0x015C, 0x0000}, /*TOP_CLKSQ_SET*/
  {0x015E, 0x0000}, /*TOP_CLKSQ_CLR*/
  {0x0160, 0x0000}, /*TOP_RST_CON*/
  {0x0162, 0x0000}, /*TOP_RST_CON_SET*/
  {0x0164, 0x0000}, /*TOP_RST_CON_CLR*/
  {0x0166, 0x0300}, /*TOP_RST_MISC*/
  {0x0168, 0x0000}, /*TOP_RST_MISC_SET*/
  {0x016A, 0x0000}, /*TOP_RST_MISC_CLR*/
  {0x016C, 0x0005}, /*INT_CON0*/
  {0x016E, 0x0000}, /*INT_CON0_SET*/
  {0x0170, 0x0000}, /*INT_CON0_CLR*/
  {0x0172, 0x1080}, /*INT_CON1*/
  {0x0174, 0x0000}, /*INT_CON1_SET*/
  {0x0176, 0x0000}, /*INT_CON1_CLR*/
  {0x0178, 0x0000}, /*INT_MISC_CON*/
  {0x017A, 0x0000}, /*INT_MISC_CON_SET*/
  {0x017C, 0x0000}, /*INT_MISC_CON_CLR*/
  {0x017E, 0x0000}, /*INT_STATUS0*/
  {0x0180, 0x0000}, /*INT_STATUS1*/
  {0x0182, 0x0001}, /*OC_GEAR_0*/
  {0x0184, 0x0000}, /*FQMTR_CON0*/
  {0x0186, 0x0000}, /*FQMTR_CON1*/
  {0x0188, 0x0000}, /*FQMTR_CON2*/
  {0x018A, 0x0000}, /*RG_SPI_CON*/
  {0x018C, 0x0000}, /*DEW_DIO_EN*/
  {0x018E, 0x5AA5}, /*DEW_READ_TEST*/
  {0x0190, 0x0000}, /*DEW_WRITE_TEST*/
  {0x0192, 0x0000}, /*DEW_CRC_SWRST*/
  {0x0194, 0x0000}, /*DEW_CRC_EN*/
  {0x0196, 0x0083}, /*DEW_CRC_VAL*/
  {0x0198, 0x0000}, /*DEW_DBG_MON_SEL*/
  {0x019A, 0x0000}, /*DEW_CIPHER_KEY_SEL*/
  {0x019C, 0x0000}, /*DEW_CIPHER_IV_SEL*/
  {0x019E, 0x0000}, /*DEW_CIPHER_EN*/
  {0x01A0, 0x0000}, /*DEW_CIPHER_RDY*/
  {0x01A2, 0x0000}, /*DEW_CIPHER_MODE*/
  {0x01A4, 0x0000}, /*DEW_CIPHER_SWRST*/
  {0x01A6, 0x000F}, /*DEW_RDDMY_NO*/
  {0x01A8, 0x0000}, /*INT_TYPE_CON0*/
  {0x01AA, 0x0000}, /*INT_TYPE_CON0_SET*/
  {0x01AC, 0x0000}, /*INT_TYPE_CON0_CLR*/
  {0x01AE, 0x0000}, /*INT_TYPE_CON1*/
  {0x01B0, 0x0000}, /*INT_TYPE_CON1_SET*/
  {0x01B2, 0x0000}, /*INT_TYPE_CON1_CLR*/
  {0x01B4, 0x0000}, /*INT_STA*/
  {0x0200, 0x0000}, /*BUCK_ALL_CON0*/
  {0x0202, 0xB100}, /*BUCK_ALL_CON1*/
  {0x0204, 0x110A}, /*BUCK_ALL_CON2*/
  {0x0206, 0xC10B}, /*BUCK_ALL_CON3*/
  {0x0208, 0x0000}, /*BUCK_ALL_CON4*/
  {0x020A, 0x0000}, /*BUCK_ALL_CON5*/
  {0x020C, 0x0000}, /*BUCK_ALL_CON6*/
  {0x020E, 0x0000}, /*BUCK_ALL_CON7*/
  {0x0210, 0x0000}, /*BUCK_ALL_CON8*/
  {0x0212, 0x000A}, /*BUCK_ALL_CON9*/
  {0x0214, 0x000A}, /*BUCK_ALL_CON10*/
  {0x0216, 0x000A}, /*BUCK_ALL_CON11*/
  {0x0218, 0x000A}, /*BUCK_ALL_CON12*/
  {0x021A, 0x000A}, /*BUCK_ALL_CON13*/
  {0x021C, 0x000A}, /*BUCK_ALL_CON14*/
  {0x021E, 0x000A}, /*BUCK_ALL_CON15*/
  {0x0220, 0x000A}, /*BUCK_ALL_CON16*/
  {0x0222, 0x0000}, /*BUCK_ALL_CON17*/
  {0x0224, 0x0000}, /*BUCK_ALL_CON18*/
  {0x0226, 0x00FF}, /*BUCK_ALL_CON19*/
  {0x0228, 0x0000}, /*BUCK_ALL_CON20*/
  {0x022A, 0x0000}, /*BUCK_ALL_CON21*/
  {0x022C, 0x0000}, /*BUCK_ALL_CON22*/
  {0x022E, 0x0000}, /*BUCK_ALL_CON23*/
  {0x0230, 0x0000}, /*BUCK_ALL_CON24*/
  {0x0232, 0x0000}, /*BUCK_ALL_CON25*/
  {0x0234, 0x0000}, /*BUCK_ALL_CON26*/
  {0x0236, 0x0000}, /*VDVFS11_CON0*/
  {0x0238, 0x0000}, /*VDVFS11_CON1*/
  {0x023A, 0x0000}, /*VDVFS11_CON2*/
  {0x023C, 0x0380}, /*VDVFS11_CON3*/
  {0x023E, 0x0238}, /*VDVFS11_CON4*/
  {0x0240, 0x0003}, /*VDVFS11_CON5*/
  {0x0242, 0x00F0}, /*VDVFS11_CON6*/
  {0x0244, 0x0008}, /*VDVFS11_CON7*/
  {0x0246, 0x3000}, /*VDVFS11_CON8*/
  {0x0248, 0x3021}, /*VDVFS11_CON9*/
  {0x024A, 0x0808}, /*VDVFS11_CON10*/
  {0x024C, 0x0030}, /*VDVFS11_CON11*/
  {0x024E, 0x0030}, /*VDVFS11_CON12*/
  {0x0250, 0x0030}, /*VDVFS11_CON13*/
  {0x0252, 0x0030}, /*VDVFS11_CON14*/
  {0x025A, 0x0001}, /*VDVFS11_CON18*/
  {0x025C, 0x10C7}, /*VDVFS11_CON19*/
  {0x025E, 0x00FF}, /*VDVFS11_CON20*/
  {0x0260, 0xFC00}, /*VDVFS11_CON21*/
  {0x0262, 0x0000}, /*VDVFS11_CON22*/
  {0x0264, 0x0032}, /*VDVFS11_CON23*/
  {0x0266, 0x0032}, /*VDVFS11_CON24*/
  {0x0268, 0x0032}, /*VDVFS11_CON25*/
  {0x026A, 0x0808}, /*VDVFS11_CON26*/
  {0x026C, 0x0001}, /*VDVFS11_CON27*/
  {0x026E, 0x0000}, /*VDVFS12_CON0*/
  {0x0270, 0x0000}, /*VDVFS12_CON1*/
  {0x0272, 0x0000}, /*VDVFS12_CON2*/
  {0x0274, 0x0340}, /*VDVFS12_CON3*/
  {0x0276, 0x0238}, /*VDVFS12_CON4*/
  {0x0278, 0x0003}, /*VDVFS12_CON5*/
  {0x027A, 0x00F0}, /*VDVFS12_CON6*/
  {0x027C, 0x0008}, /*VDVFS12_CON7*/
  {0x027E, 0x3000}, /*VDVFS12_CON8*/
  {0x0280, 0x3021}, /*VDVFS12_CON9*/
  {0x0282, 0x0808}, /*VDVFS12_CON10*/
  {0x0284, 0x0030}, /*VDVFS12_CON11*/
  {0x0286, 0x0030}, /*VDVFS12_CON12*/
  {0x0288, 0x0030}, /*VDVFS12_CON13*/
  {0x028A, 0x0030}, /*VDVFS12_CON14*/
  {0x0292, 0x0001}, /*VDVFS12_CON18*/
  {0x0294, 0x08C7}, /*VDVFS12_CON19*/
  {0x0296, 0x00FF}, /*VDVFS12_CON20*/
  {0x0298, 0x0000}, /*VDVFS13_CON0*/
  {0x029A, 0x0000}, /*VDVFS13_CON1*/
  {0x029C, 0x0000}, /*VDVFS13_CON2*/
  {0x029E, 0x0380}, /*VDVFS13_CON3*/
  {0x02A0, 0x0238}, /*VDVFS13_CON4*/
  {0x02A2, 0x0003}, /*VDVFS13_CON5*/
  {0x02A4, 0x00F0}, /*VDVFS13_CON6*/
  {0x02A6, 0x0008}, /*VDVFS13_CON7*/
  {0x02A8, 0x3000}, /*VDVFS13_CON8*/
  {0x02AA, 0x3021}, /*VDVFS13_CON9*/
  {0x02AC, 0x0808}, /*VDVFS13_CON10*/
  {0x02AE, 0x0030}, /*VDVFS13_CON11*/
  {0x02B0, 0x0030}, /*VDVFS13_CON12*/
  {0x02B2, 0x0030}, /*VDVFS13_CON13*/
  {0x02B4, 0x0030}, /*VDVFS13_CON14*/
  {0x02BC, 0x0001}, /*VDVFS13_CON18*/
  {0x02BE, 0x10C7}, /*VDVFS13_CON19*/
  {0x02C0, 0x00FF}, /*VDVFS13_CON20*/
  {0x02C2, 0x0000}, /*VDVFS14_CON0*/
  {0x02C4, 0x0000}, /*VDVFS14_CON1*/
  {0x02C6, 0x0000}, /*VDVFS14_CON2*/
  {0x02C8, 0x0340}, /*VDVFS14_CON3*/
  {0x02CA, 0x0238}, /*VDVFS14_CON4*/
  {0x02CC, 0x0003}, /*VDVFS14_CON5*/
  {0x02CE, 0x00F0}, /*VDVFS14_CON6*/
  {0x02D0, 0x0008}, /*VDVFS14_CON7*/
  {0x02D2, 0x3000}, /*VDVFS14_CON8*/
  {0x02D4, 0x3021}, /*VDVFS14_CON9*/
  {0x02D6, 0x0808}, /*VDVFS14_CON10*/
  {0x02D8, 0x0030}, /*VDVFS14_CON11*/
  {0x02DA, 0x0030}, /*VDVFS14_CON12*/
  {0x02DC, 0x0030}, /*VDVFS14_CON13*/
  {0x02DE, 0x0030}, /*VDVFS14_CON14*/
  {0x02E6, 0x0001}, /*VDVFS14_CON18*/
  {0x02E8, 0x08C7}, /*VDVFS14_CON19*/
  {0x02EA, 0x00FF}, /*VDVFS14_CON20*/
  {0x0300, 0x0000}, /*VGPU_CON0*/
  {0x0302, 0x0000}, /*VGPU_CON1*/
  {0x0304, 0x0000}, /*VGPU_CON2*/
  {0x0306, 0x0746}, /*VGPU_CON3*/
  {0x0308, 0x0200}, /*VGPU_CON4*/
  {0x030A, 0x0001}, /*VGPU_CON5*/
  {0x030C, 0x00F0}, /*VGPU_CON6*/
  {0x030E, 0x0000}, /*VGPU_CON7*/
  {0x0310, 0x0000}, /*VGPU_CON8*/
  {0x0312, 0x0020}, /*VGPU_CON9*/
  {0x0314, 0x0808}, /*VGPU_CON10*/
  {0x0316, 0x0038}, /*VGPU_CON11*/
  {0x0318, 0x0038}, /*VGPU_CON12*/
  {0x031A, 0x0038}, /*VGPU_CON13*/
  {0x031C, 0x0038}, /*VGPU_CON14*/
  {0x031E, 0x3333}, /*VGPU_CON15*/
  {0x0320, 0x3333}, /*VGPU_CON16*/
  {0x0322, 0x3333}, /*VGPU_CON17*/
  {0x0324, 0x0001}, /*VGPU_CON18*/
  {0x0326, 0x0016}, /*VGPU_CON19*/
  {0x0328, 0x0080}, /*VGPU_CON20*/
  {0x032A, 0x0000}, /*VCORE1_CON0*/
  {0x032C, 0x0000}, /*VCORE1_CON1*/
  {0x032E, 0x0000}, /*VCORE1_CON2*/
  {0x0330, 0x0746}, /*VCORE1_CON3*/
  {0x0332, 0x0200}, /*VCORE1_CON4*/
  {0x0334, 0x0001}, /*VCORE1_CON5*/
  {0x0336, 0x00F0}, /*VCORE1_CON6*/
  {0x0338, 0x0000}, /*VCORE1_CON7*/
  {0x033A, 0x0000}, /*VCORE1_CON8*/
  {0x033C, 0x0020}, /*VCORE1_CON9*/
  {0x033E, 0x0808}, /*VCORE1_CON10*/
  {0x0340, 0x0030}, /*VCORE1_CON11*/
  {0x0342, 0x0030}, /*VCORE1_CON12*/
  {0x0344, 0x0030}, /*VCORE1_CON13*/
  {0x0346, 0x0030}, /*VCORE1_CON14*/
  {0x0348, 0x3333}, /*VCORE1_CON15*/
  {0x034A, 0x3333}, /*VCORE1_CON16*/
  {0x034C, 0x3333}, /*VCORE1_CON17*/
  {0x034E, 0x0001}, /*VCORE1_CON18*/
  {0x0350, 0x0016}, /*VCORE1_CON19*/
  {0x0352, 0x0080}, /*VCORE1_CON20*/
  {0x0354, 0x0000}, /*VCORE2_CON0*/
  {0x0356, 0x0000}, /*VCORE2_CON1*/
  {0x0358, 0x0000}, /*VCORE2_CON2*/
  {0x035A, 0x0742}, /*VCORE2_CON3*/
  {0x035C, 0x0200}, /*VCORE2_CON4*/
  {0x035E, 0x0002}, /*VCORE2_CON5*/
  {0x0360, 0x00F0}, /*VCORE2_CON6*/
  {0x0362, 0x0000}, /*VCORE2_CON7*/
  {0x0364, 0x0000}, /*VCORE2_CON8*/
  {0x0366, 0x3021}, /*VCORE2_CON9*/
  {0x0368, 0x0808}, /*VCORE2_CON10*/
  {0x036A, 0x0038}, /*VCORE2_CON11*/
  {0x036C, 0x0038}, /*VCORE2_CON12*/
  {0x036E, 0x0038}, /*VCORE2_CON13*/
  {0x0370, 0x0038}, /*VCORE2_CON14*/
  {0x0372, 0x5335}, /*VCORE2_CON15*/
  {0x0374, 0x3333}, /*VCORE2_CON16*/
  {0x0376, 0x3333}, /*VCORE2_CON17*/
  {0x0378, 0x0001}, /*VCORE2_CON18*/
  {0x037A, 0x0017}, /*VCORE2_CON19*/
  {0x037C, 0x0080}, /*VCORE2_CON20*/
  {0x037E, 0x0000}, /*VCORE2_CON21*/
  {0x0380, 0x0000}, /*VIO18_CON0*/
  {0x0382, 0x0000}, /*VIO18_CON1*/
  {0x0384, 0x0000}, /*VIO18_CON2*/
  {0x0386, 0x0C86}, /*VIO18_CON3*/
  {0x0388, 0x0250}, /*VIO18_CON4*/
  {0x038A, 0x0011}, /*VIO18_CON5*/
  {0x038C, 0x00F0}, /*VIO18_CON6*/
  {0x038E, 0x0000}, /*VIO18_CON7*/
  {0x0390, 0x0000}, /*VIO18_CON8*/
  {0x0392, 0x3021}, /*VIO18_CON9*/
  {0x0394, 0x0808}, /*VIO18_CON10*/
  {0x0396, 0x0024}, /*VIO18_CON11*/
  {0x0398, 0x0024}, /*VIO18_CON12*/
  {0x039A, 0x0024}, /*VIO18_CON13*/
  {0x039C, 0x0024}, /*VIO18_CON14*/
  {0x039E, 0x2222}, /*VIO18_CON15*/
  {0x03A0, 0x3333}, /*VIO18_CON16*/
  {0x03A2, 0x3333}, /*VIO18_CON17*/
  {0x03A4, 0x0001}, /*VIO18_CON18*/
  {0x03A6, 0x007B}, /*VIO18_CON19*/
  {0x03A8, 0x0080}, /*VIO18_CON20*/
  {0x03AA, 0x0000}, /*BUCK_K_CON0*/
  {0x03AC, 0x0000}, /*BUCK_K_CON1*/
  {0x03AE, 0x0000}, /*BUCK_K_CON2*/
  {0x03B0, 0x016F}, /*BUCK_K_CON3*/
  {0x0400, 0x0000}, /*ZCD_CON0*/
  {0x0402, 0x0F9F}, /*ZCD_CON1*/
  {0x0404, 0x0F9F}, /*ZCD_CON2*/
  {0x0406, 0x001F}, /*ZCD_CON3*/
  {0x0408, 0x0707}, /*ZCD_CON4*/
  {0x040A, 0x3F3F}, /*ZCD_CON5*/
  {0x040C, 0x0000}, /*ISINK0_CON0*/
  {0x040E, 0x0000}, /*ISINK0_CON1*/
  {0x0410, 0x0000}, /*ISINK0_CON2*/
  {0x0412, 0x0000}, /*ISINK0_CON3*/
  {0x0414, 0x0000}, /*ISINK0_CON4*/
  {0x0416, 0x0000}, /*ISINK1_CON0*/
  {0x0418, 0x0000}, /*ISINK1_CON1*/
  {0x041A, 0x0000}, /*ISINK1_CON2*/
  {0x041C, 0x0000}, /*ISINK1_CON3*/
  {0x041E, 0x0000}, /*ISINK1_CON4*/
  {0x0420, 0x0000}, /*ISINK2_CON0*/
  {0x0422, 0x0000}, /*ISINK2_CON1*/
  {0x0424, 0x0000}, /*ISINK2_CON2*/
  {0x0426, 0x0000}, /*ISINK2_CON3*/
  {0x0428, 0x0000}, /*ISINK2_CON4*/
  {0x042A, 0x0000}, /*ISINK3_CON0*/
  {0x042C, 0x0000}, /*ISINK3_CON1*/
  {0x042E, 0x0000}, /*ISINK3_CON2*/
  {0x0430, 0x0000}, /*ISINK3_CON3*/
  {0x0432, 0x0000}, /*ISINK3_CON4*/
  {0x0434, 0x0000}, /*ISINK_ANA0*/
  {0x0436, 0x0000}, /*ISINK_ANA1*/
  {0x0438, 0x0000}, /*ISINK_PHASE_DLY*/
  {0x043A, 0x0000}, /*ISINK_EN_CTRL*/
  {0x0500, 0x0000}, /*ANALDO_CON0*/
  {0x0502, 0xC540}, /*ANALDO_CON1*/
  {0x0504, 0x4140}, /*ANALDO_CON2*/
  {0x0506, 0xC540}, /*ANALDO_CON3*/
  {0x0508, 0xC540}, /*ANALDO_CON4*/
  {0x050A, 0x0100}, /*ANALDO_CON5*/
  {0x050C, 0x0064}, /*ANALDO_CON6*/
  {0x050E, 0x0064}, /*ANALDO_CON7*/
  {0x0510, 0x0064}, /*ANALDO_CON8*/
  {0x0512, 0x0034}, /*ANALDO_CON9*/
  {0x0514, 0x0064}, /*ANALDO_CON10*/
  {0x0516, 0x0000}, /*ANALDO_CON11*/
  {0x0518, 0x0000}, /*ANALDO_CON12*/
  {0x051A, 0x0000}, /*ANALDO_CON13*/
  {0x051C, 0x0000}, /*SYSLDO_CON0*/
  {0x051E, 0x0140}, /*SYSLDO_CON1*/
  {0x0520, 0x8540}, /*SYSLDO_CON2*/
  {0x0522, 0x0140}, /*SYSLDO_CON3*/
  {0x0524, 0x8500}, /*SYSLDO_CON4*/
  {0x0526, 0x4140}, /*SYSLDO_CON5*/
  {0x0528, 0x0140}, /*SYSLDO_CON6*/
  {0x052A, 0x0140}, /*SYSLDO_CON7*/
  {0x052C, 0x8500}, /*SYSLDO_CON8*/
  {0x052E, 0x0044}, /*SYSLDO_CON9*/
  {0x0530, 0x0004}, /*SYSLDO_CON10*/
  {0x0532, 0x001C}, /*SYSLDO_CON11*/
  {0x0534, 0x6484}, /*SYSLDO_CON12*/
  {0x0536, 0x001C}, /*SYSLDO_CON13*/
  {0x0538, 0x001C}, /*SYSLDO_CON14*/
  {0x053A, 0x001C}, /*SYSLDO_CON15*/
  {0x053C, 0x0004}, /*SYSLDO_CON16*/
  {0x053E, 0x0007}, /*SYSLDO_CON17*/
  {0x0540, 0x0000}, /*SYSLDO_CON18*/
  {0x0542, 0x0000}, /*SYSLDO_CON19*/
  {0x0544, 0x0000}, /*SYSLDO_CON20*/
  {0x0546, 0x0000}, /*SYSLDO_CON21*/
  {0x0548, 0x0000}, /*DIGLDO_CON0*/
  {0x054A, 0x8540}, /*DIGLDO_CON1*/
  {0x054C, 0x0140}, /*DIGLDO_CON2*/
  {0x054E, 0xC548}, /*DIGLDO_CON3*/
  {0x0550, 0xC540}, /*DIGLDO_CON4*/
  {0x0552, 0xC540}, /*DIGLDO_CON5*/
  {0x0554, 0x0140}, /*DIGLDO_CON6*/
  {0x0556, 0x0140}, /*DIGLDO_CON7*/
  {0x0558, 0x4140}, /*DIGLDO_CON8*/
  {0x055A, 0x4140}, /*DIGLDO_CON9*/
  {0x055C, 0x0000}, /*DIGLDO_CON10*/
  {0x055E, 0x8100}, /*DIGLDO_CON11*/
  {0x0560, 0x0140}, /*DIGLDO_CON12*/
  {0x0562, 0x0004}, /*DIGLDO_CON13*/
  {0x0564, 0x0054}, /*DIGLDO_CON14*/
  {0x0566, 0x0014}, /*DIGLDO_CON15*/
  {0x0568, 0x0044}, /*DIGLDO_CON16*/
  {0x056A, 0x0044}, /*DIGLDO_CON17*/
  {0x056C, 0x0054}, /*DIGLDO_CON18*/
  {0x056E, 0x0054}, /*DIGLDO_CON19*/
  {0x0570, 0x0054}, /*DIGLDO_CON20*/
  {0x0572, 0x0004}, /*DIGLDO_CON21*/
  {0x0574, 0x0004}, /*DIGLDO_CON22*/
  {0x0576, 0x0094}, /*DIGLDO_CON23*/
  {0x0578, 0x0000}, /*DIGLDO_CON24*/
  {0x057A, 0x0000}, /*DIGLDO_CON25*/
  {0x057C, 0x0005}, /*DIGLDO_CON26*/
  {0x057E, 0x0000}, /*DIGLDO_CON27*/
  {0x0580, 0x6300}, /*DIGLDO_CON28*/
  {0x0600, 0x0000}, /*OTP_CON0*/
  {0x0602, 0x0000}, /*OTP_CON1*/
  {0x0604, 0x0000}, /*OTP_CON2*/
  {0x0606, 0x0000}, /*OTP_CON3*/
  {0x0608, 0x0000}, /*OTP_CON4*/
  {0x060A, 0x0000}, /*OTP_CON5*/
  {0x060C, 0x0000}, /*OTP_CON6*/
  {0x060E, 0x0000}, /*OTP_CON7*/
  {0x0610, 0x0000}, /*OTP_CON8*/
  {0x0612, 0x0000}, /*OTP_CON9*/
  {0x0614, 0x0000}, /*OTP_CON10*/
  {0x0616, 0x0000}, /*OTP_CON11*/
  {0x0618, 0x0000}, /*OTP_CON12*/
  {0x061A, 0x0000}, /*OTP_CON13*/
  {0x061C, 0x0000}, /*OTP_CON14*/
  {0x061E, 0x0000}, /*OTP_DOUT_0_15*/
  {0x0620, 0x0000}, /*OTP_DOUT_16_31*/
  {0x0622, 0x0000}, /*OTP_DOUT_32_47*/
  {0x0624, 0x0000}, /*OTP_DOUT_48_63*/
  {0x0626, 0x0000}, /*OTP_DOUT_64_79*/
  {0x0628, 0x0000}, /*OTP_DOUT_80_95*/
  {0x062A, 0x0000}, /*OTP_DOUT_96_111*/
  {0x062C, 0x0000}, /*OTP_DOUT_112_127*/
  {0x062E, 0x0000}, /*OTP_DOUT_128_143*/
  {0x0630, 0x0000}, /*OTP_DOUT_144_159*/
  {0x0632, 0x0000}, /*OTP_DOUT_160_175*/
  {0x0634, 0x0000}, /*OTP_DOUT_176_191*/
  {0x0636, 0x0000}, /*OTP_DOUT_192_207*/
  {0x0638, 0x0000}, /*OTP_DOUT_208_223*/
  {0x063A, 0x0000}, /*OTP_DOUT_224_239*/
  {0x063C, 0x0000}, /*OTP_DOUT_240_255*/
  {0x063E, 0x0006}, /*OTP_VAL_0_15*/
  {0x0640, 0x0000}, /*OTP_VAL_16_31*/
  {0x0642, 0x0000}, /*OTP_VAL_32_47*/
  {0x0644, 0x0C00}, /*OTP_VAL_48_63*/
  {0x0646, 0x3800}, /*OTP_VAL_64_79*/
  {0x0648, 0x0000}, /*OTP_VAL_80_95*/
  {0x064A, 0x0000}, /*OTP_VAL_96_111*/
  {0x064C, 0x0000}, /*OTP_VAL_112_127*/
  {0x064E, 0x0000}, /*OTP_VAL_128_143*/
  {0x0650, 0x0000}, /*OTP_VAL_144_159*/
  {0x0652, 0x0064}, /*OTP_VAL_160_175*/
  {0x0654, 0x0000}, /*OTP_VAL_176_191*/
  {0x0656, 0x0000}, /*OTP_VAL_192_207*/
  {0x0658, 0x0000}, /*OTP_VAL_208_223*/
  {0x065A, 0x0000}, /*OTP_VAL_224_239*/
  {0x065C, 0x0006}, /*OTP_VAL_240_255*/
  {0x065E, 0x0788}, /*RTC_MIX_CON0*/
  {0x0660, 0x0200}, /*RTC_MIX_CON1*/
  {0x0662, 0x0000}, /*AUDDAC_CFG0*/
  {0x0664, 0x0000}, /*AUDBUF_CFG0*/
  {0x0666, 0x0000}, /*AUDBUF_CFG1*/
  {0x0668, 0x0020}, /*AUDBUF_CFG2*/
  {0x066A, 0x0000}, /*AUDBUF_CFG3*/
  {0x066C, 0x0000}, /*AUDBUF_CFG4*/
  {0x066E, 0x0000}, /*AUDBUF_CFG5*/
  {0x0670, 0x0000}, /*AUDBUF_CFG6*/
  {0x0672, 0x0100}, /*AUDBUF_CFG7*/
  {0x0674, 0x0000}, /*AUDBUF_CFG8*/
  {0x0676, 0x0292}, /*IBIASDIST_CFG0*/
  {0x0678, 0x5500}, /*AUDCLKGEN_CFG0*/
  {0x067A, 0x0000}, /*AUDLDO_CFG0*/
  {0x067C, 0x0026}, /*AUDDCDC_CFG0*/
  {0x067E, 0x0000}, /*AUDDCDC_CFG1*/
  {0x0680, 0x0000}, /*AUDNVREGGLB_CFG0*/
  {0x0682, 0x0000}, /*AUD_NCP0*/
  {0x0684, 0x0000}, /*AUD_ZCD_CFG0*/
  {0x0686, 0x0000}, /*AUDPREAMP_CFG0*/
  {0x0688, 0x0000}, /*AUDPREAMP_CFG1*/
  {0x068A, 0x0000}, /*AUDPREAMP_CFG2*/
  {0x068C, 0x0000}, /*AUDADC_CFG0*/
  {0x068E, 0x0400}, /*AUDADC_CFG1*/
  {0x0690, 0x0000}, /*AUDADC_CFG2*/
  {0x0692, 0x1515}, /*AUDADC_CFG3*/
  {0x0694, 0x1515}, /*AUDADC_CFG4*/
  {0x0696, 0x0000}, /*AUDADC_CFG5*/
  {0x0698, 0x0040}, /*AUDDIGMI_CFG0*/
  {0x069A, 0x0040}, /*AUDDIGMI_CFG1*/
  {0x069C, 0x0000}, /*AUDMICBIAS_CFG0*/
  {0x069E, 0x0000}, /*AUDMICBIAS_CFG1*/
  {0x06A0, 0x0000}, /*AUDENCSPARE_CFG0*/
  {0x06A2, 0x0000}, /*AUDPREAMPGAIN_CFG0*/
  {0x06A4, 0x00C4}, /*AUDMADPLL_CFG0*/
  {0x06A6, 0x0180}, /*AUDMADPLL_CFG1*/
  {0x06A8, 0x0023}, /*AUDMADPLL_CFG2*/
  {0x06AA, 0x0000}, /*AUDLDO_NVREG_CFG0*/
  {0x06AC, 0x0000}, /*AUDLDO_NVREG_CFG1*/
  {0x06AE, 0x0000}, /*AUDLDO_NVREG_CFG2*/
  {0x0700, 0x0000}, /*AUXADC_ADC0*/
  {0x0702, 0x0000}, /*AUXADC_ADC1*/
  {0x0704, 0x0000}, /*AUXADC_ADC2*/
  {0x0706, 0x0000}, /*AUXADC_ADC3*/
  {0x0708, 0x0000}, /*AUXADC_ADC4*/
  {0x070A, 0x0000}, /*AUXADC_ADC5*/
  {0x070C, 0x0000}, /*AUXADC_ADC6*/
  {0x070E, 0x0000}, /*AUXADC_ADC7*/
  {0x0710, 0x0000}, /*AUXADC_ADC8*/
  {0x0712, 0x0000}, /*AUXADC_ADC9*/
  {0x0714, 0x0000}, /*AUXADC_ADC10*/
  {0x0716, 0x0000}, /*AUXADC_ADC11*/
  {0x0718, 0x0000}, /*AUXADC_ADC12*/
  {0x071A, 0x0000}, /*AUXADC_ADC13*/
  {0x071C, 0x0000}, /*AUXADC_ADC14*/
  {0x071E, 0x0000}, /*AUXADC_ADC15*/
  {0x0720, 0x0000}, /*AUXADC_ADC16*/
  {0x0722, 0x0000}, /*AUXADC_ADC17*/
  {0x0724, 0x0000}, /*AUXADC_ADC18*/
  {0x0726, 0x0000}, /*AUXADC_ADC19*/
  {0x0728, 0x0000}, /*AUXADC_STA0*/
  {0x072A, 0x0000}, /*AUXADC_STA1*/
  {0x072C, 0x0000}, /*AUXADC_RQST0*/
  {0x072E, 0x0000}, /*AUXADC_RQST0_SET*/
  {0x0730, 0x0000}, /*AUXADC_RQST0_CLR*/
  {0x0732, 0x0000}, /*AUXADC_RQST1*/
  {0x0734, 0x0000}, /*AUXADC_RQST1_SET*/
  {0x0736, 0x0000}, /*AUXADC_RQST1_CLR*/
  {0x0738, 0x8014}, /*AUXADC_CON0*/
  {0x073A, 0x00B0}, /*AUXADC_CON1*/
  {0x073C, 0x0080}, /*AUXADC_CON2*/
  {0x073E, 0x2040}, /*AUXADC_CON3*/
  {0x0740, 0x4000}, /*AUXADC_CON4*/
  {0x0742, 0x0000}, /*AUXADC_CON5*/
  {0x0744, 0x0000}, /*AUXADC_CON6*/
  {0x0746, 0x0000}, /*AUXADC_CON7*/
  {0x0748, 0x0000}, /*AUXADC_CON8*/
  {0x074A, 0x0000}, /*AUXADC_CON9*/
  {0x074C, 0x0000}, /*AUXADC_CON10*/
  {0x074E, 0x01E0}, /*AUXADC_CON11*/
  {0x0750, 0x07D4}, /*AUXADC_CON12*/
  {0x0752, 0x0020}, /*AUXADC_CON13*/
  {0x0754, 0x0000}, /*AUXADC_CON14*/
  {0x0756, 0x0000}, /*AUXADC_CON15*/
  {0x0758, 0x0000}, /*AUXADC_CON16*/
  {0x075A, 0x8000}, /*AUXADC_CON17*/
  {0x075C, 0x8000}, /*AUXADC_CON18*/
  {0x075E, 0x0000}, /*AUXADC_CON19*/
  {0x0760, 0x0000}, /*AUXADC_CON20*/
  {0x0762, 0x00FF}, /*AUXADC_CON21*/
  {0x0764, 0x0000}, /*AUXADC_CON22*/
  {0x0766, 0x0000}, /*AUXADC_CON23*/
  {0x0768, 0x0000}, /*AUXADC_CON24*/
  {0x076A, 0x8000}, /*AUXADC_CON25*/
  {0x076C, 0x8000}, /*AUXADC_CON26*/
  {0x076E, 0x0000}, /*AUXADC_CON27*/
  {0x0770, 0x0000}, /*AUXADC_CON28*/
  {0x077A, 0x0010}, /*ACCDET_CON0*/
  {0x077C, 0x0000}, /*ACCDET_CON1*/
  {0x077E, 0x0000}, /*ACCDET_CON2*/
  {0x0780, 0x0000}, /*ACCDET_CON3*/
  {0x0782, 0x0000}, /*ACCDET_CON4*/
  {0x0784, 0x0101}, /*ACCDET_CON5*/
  {0x0786, 0x0010}, /*ACCDET_CON6*/
  {0x0788, 0x0010}, /*ACCDET_CON7*/
  {0x078A, 0x0010}, /*ACCDET_CON8*/
  {0x078C, 0x0010}, /*ACCDET_CON9*/
  {0x078E, 0x0005}, /*ACCDET_CON10*/
  {0x0790, 0x0333}, /*ACCDET_CON11*/
  {0x0792, 0x8000}, /*ACCDET_CON12*/
  {0x0794, 0x0000}, /*ACCDET_CON13*/
  {0x0796, 0x00FF}, /*ACCDET_CON14*/
  {0x0798, 0x0050}, /*ACCDET_CON15*/
  {0x079A, 0x0161}, /*ACCDET_CON16*/
  {0x079C, 0x0000}, /*ACCDET_CON17*/
  {0x079E, 0x0000}, /*ACCDET_CON18*/
  {0x07A0, 0x0000}, /*ACCDET_CON19*/
  {0x07A2, 0x0000}, /*ACCDET_CON20*/
  {0x07A4, 0x0010}, /*ACCDET_CON21*/
  {0x07A6, 0x03FF}, /*ACCDET_CON22*/
  {0x07A8, 0x0000}, /*ACCDET_CON23*/
  {0x07AA, 0x0000}, /*ACCDET_CON24*/
};

static kal_uint32 mt6331_aPMURegDef_mask[569] = { /* mask*/
  0x007F, /*STRUP_CON0*/
  0x001F, /*STRUP_CON2*/
  0x7037, /*STRUP_CON3*/
  0x000F, /*STRUP_CON4*/
  0x0003, /*STRUP_CON5*/
  0xBFFF, /*STRUP_CON6*/
  0x1FFF, /*STRUP_CON7*/
  0x7FFF, /*STRUP_CON8*/
  0x4030, /*STRUP_CON9*/
  0x7F03, /*STRUP_CON10*/
  0x00F0, /*STRUP_CON11*/
  0x0003, /*STRUP_CON12*/
  0xFFFF, /*STRUP_CON13*/
  0xFFE3, /*STRUP_CON14*/
  0x00FF, /*STRUP_CON15*/
  0xF80F, /*STRUP_CON16*/
  0xFFFF, /*STRUP_CON17*/
  0x8000, /*STRUP_CON18*/
  0xFFFF, /*HWCID*/
  0xFFFF, /*SWCID*/
  0x8000, /*EXT_PMIC_STATUS*/
  0x0377, /*TOP_CON*/
  0x0000, /*TEST_OUT*/
  0x0FFF, /*TEST_CON0*/
  0x000F, /*TEST_CON1*/
  0x0001, /*TESTMODE_SW*/
  0x0000, /*EN_STATUS0*/
  0x0000, /*EN_STATUS1*/
  0x0000, /*EN_STATUS2*/
  0x0000, /*OCSTATUS0*/
  0x0000, /*OCSTATUS1*/
  0x0000, /*OCSTATUS2*/
  0x0000, /*PGSTATUS*/
  0x0F00, /*TOPSTATUS*/
  0x0007, /*TDSEL_CON*/
  0x0007, /*RDSEL_CON*/
  0x003F, /*SMT_CON0*/
  0x000F, /*SMT_CON1*/
  0x001F, /*SMT_CON2*/
  0xFFFF, /*DRV_CON0*/
  0xFFFF, /*DRV_CON1*/
  0xFFFF, /*DRV_CON2*/
  0x00FF, /*DRV_CON3*/
  0x000F, /*TOP_STATUS*/
  0x0000, /*TOP_STATUS_SET*/
  0x0000, /*TOP_STATUS_CLR*/
  0xFFFF, /*TOP_CKPDN_CON0*/
  0x0000, /*TOP_CKPDN_CON0_SET*/
  0x0000, /*TOP_CKPDN_CON0_CLR*/
  0xFFFF, /*TOP_CKPDN_CON1*/
  0x0000, /*TOP_CKPDN_CON1_SET*/
  0x0000, /*TOP_CKPDN_CON1_CLR*/
  0xFFFF, /*TOP_CKPDN_CON2*/
  0x0000, /*TOP_CKPDN_CON2_SET*/
  0x0000, /*TOP_CKPDN_CON2_CLR*/
  0xF7FF, /*TOP_CKSEL_CON*/
  0x0000, /*TOP_CKSEL_CON_SET*/
  0x0000, /*TOP_CKSEL_CON_CLR*/
  0x00FF, /*TOP_CKHWEN_CON*/
  0x0000, /*TOP_CKHWEN_CON_SET*/
  0x0000, /*TOP_CKHWEN_CON_CLR*/
  0x01FF, /*TOP_CKTST_CON0*/
  0xFFFF, /*TOP_CKTST_CON1*/
  0x0003, /*TOP_CLKSQ*/
  0x0000, /*TOP_CLKSQ_SET*/
  0x0000, /*TOP_CLKSQ_CLR*/
  0xFFFF, /*TOP_RST_CON*/
  0x0000, /*TOP_RST_CON_SET*/
  0x0000, /*TOP_RST_CON_CLR*/
  0x371B, /*TOP_RST_MISC*/
  0x0000, /*TOP_RST_MISC_SET*/
  0x0000, /*TOP_RST_MISC_CLR*/
  0x1FFF, /*INT_CON0*/
  0x0000, /*INT_CON0_SET*/
  0x0000, /*INT_CON0_CLR*/
  0x1FFF, /*INT_CON1*/
  0x0000, /*INT_CON1_SET*/
  0x0000, /*INT_CON1_CLR*/
  0x0007, /*INT_MISC_CON*/
  0x0000, /*INT_MISC_CON_SET*/
  0x0000, /*INT_MISC_CON_CLR*/
  0x0000, /*INT_STATUS0*/
  0x0000, /*INT_STATUS1*/
  0x0003, /*OC_GEAR_0*/
  0x800F, /*FQMTR_CON0*/
  0xFFFF, /*FQMTR_CON1*/
  0xFFFF, /*FQMTR_CON2*/
  0x0001, /*RG_SPI_CON*/
  0x0001, /*DEW_DIO_EN*/
  0xFFFF, /*DEW_READ_TEST*/
  0xFFFF, /*DEW_WRITE_TEST*/
  0x0001, /*DEW_CRC_SWRST*/
  0x0001, /*DEW_CRC_EN*/
  0x00FF, /*DEW_CRC_VAL*/
  0x000F, /*DEW_DBG_MON_SEL*/
  0x0003, /*DEW_CIPHER_KEY_SEL*/
  0x0003, /*DEW_CIPHER_IV_SEL*/
  0x0001, /*DEW_CIPHER_EN*/
  0x0001, /*DEW_CIPHER_RDY*/
  0x0001, /*DEW_CIPHER_MODE*/
  0x0001, /*DEW_CIPHER_SWRST*/
  0x000F, /*DEW_RDDMY_NO*/
  0x1FFF, /*INT_TYPE_CON0*/
  0x0000, /*INT_TYPE_CON0_SET*/
  0x0000, /*INT_TYPE_CON0_CLR*/
  0x01FF, /*INT_TYPE_CON1*/
  0x0000, /*INT_TYPE_CON1_SET*/
  0x0000, /*INT_TYPE_CON1_CLR*/
  0x0003, /*INT_STA*/
  0xFF80, /*BUCK_ALL_CON0*/
  0xF1FF, /*BUCK_ALL_CON1*/
  0xF1FF, /*BUCK_ALL_CON2*/
  0xF1FF, /*BUCK_ALL_CON3*/
  0xFFFF, /*BUCK_ALL_CON4*/
  0x0000, /*BUCK_ALL_CON5*/
  0x0000, /*BUCK_ALL_CON6*/
  0x0000, /*BUCK_ALL_CON7*/
  0x0000, /*BUCK_ALL_CON8*/
  0x00CF, /*BUCK_ALL_CON9*/
  0x00CF, /*BUCK_ALL_CON10*/
  0x00CF, /*BUCK_ALL_CON11*/
  0x00CF, /*BUCK_ALL_CON12*/
  0x00CF, /*BUCK_ALL_CON13*/
  0x00CF, /*BUCK_ALL_CON14*/
  0x00CF, /*BUCK_ALL_CON15*/
  0x00CF, /*BUCK_ALL_CON16*/
  0x00FF, /*BUCK_ALL_CON17*/
  0xFFFF, /*BUCK_ALL_CON18*/
  0x00FF, /*BUCK_ALL_CON19*/
  0x00FF, /*BUCK_ALL_CON20*/
  0x0007, /*BUCK_ALL_CON21*/
  0x7F7F, /*BUCK_ALL_CON22*/
  0x7F7F, /*BUCK_ALL_CON23*/
  0x007F, /*BUCK_ALL_CON24*/
  0xF800, /*BUCK_ALL_CON25*/
  0x001F, /*BUCK_ALL_CON26*/
  0x3F00, /*VDVFS11_CON0*/
  0x001F, /*VDVFS11_CON1*/
  0x001F, /*VDVFS11_CON2*/
  0xC3C0, /*VDVFS11_CON3*/
  0x0378, /*VDVFS11_CON4*/
  0x0073, /*VDVFS11_CON5*/
  0x00FF, /*VDVFS11_CON6*/
  0x000F, /*VDVFS11_CON7*/
  0x3333, /*VDVFS11_CON8*/
  0xB031, /*VDVFS11_CON9*/
  0xFFFF, /*VDVFS11_CON10*/
  0x007F, /*VDVFS11_CON11*/
  0x007F, /*VDVFS11_CON12*/
  0x007F, /*VDVFS11_CON13*/
  0x007F, /*VDVFS11_CON14*/
  0xCDF3, /*VDVFS11_CON18*/
  0x3BE7, /*VDVFS11_CON19*/
  0x00FF, /*VDVFS11_CON20*/
  0xFFE7, /*VDVFS11_CON21*/
  0x0007, /*VDVFS11_CON22*/
  0x007F, /*VDVFS11_CON23*/
  0x007F, /*VDVFS11_CON24*/
  0x007F, /*VDVFS11_CON25*/
  0xFFFF, /*VDVFS11_CON26*/
  0xCDF3, /*VDVFS11_CON27*/
  0x3F00, /*VDVFS12_CON0*/
  0x001F, /*VDVFS12_CON1*/
  0x001F, /*VDVFS12_CON2*/
  0xC3C0, /*VDVFS12_CON3*/
  0x0378, /*VDVFS12_CON4*/
  0x0073, /*VDVFS12_CON5*/
  0x00FF, /*VDVFS12_CON6*/
  0x000F, /*VDVFS12_CON7*/
  0x3333, /*VDVFS12_CON8*/
  0xB031, /*VDVFS12_CON9*/
  0xFFFF, /*VDVFS12_CON10*/
  0x007F, /*VDVFS12_CON11*/
  0x007F, /*VDVFS12_CON12*/
  0x007F, /*VDVFS12_CON13*/
  0x007F, /*VDVFS12_CON14*/
  0xCDF3, /*VDVFS12_CON18*/
  0x3BE7, /*VDVFS12_CON19*/
  0x00FF, /*VDVFS12_CON20*/
  0x3F00, /*VDVFS13_CON0*/
  0x001F, /*VDVFS13_CON1*/
  0x001F, /*VDVFS13_CON2*/
  0xC3C0, /*VDVFS13_CON3*/
  0x0378, /*VDVFS13_CON4*/
  0x0073, /*VDVFS13_CON5*/
  0x00FF, /*VDVFS13_CON6*/
  0x000F, /*VDVFS13_CON7*/
  0x3333, /*VDVFS13_CON8*/
  0xB031, /*VDVFS13_CON9*/
  0xFFFF, /*VDVFS13_CON10*/
  0x007F, /*VDVFS13_CON11*/
  0x007F, /*VDVFS13_CON12*/
  0x007F, /*VDVFS13_CON13*/
  0x007F, /*VDVFS13_CON14*/
  0xCDF3, /*VDVFS13_CON18*/
  0x3BE7, /*VDVFS13_CON19*/
  0x00FF, /*VDVFS13_CON20*/
  0x3F00, /*VDVFS14_CON0*/
  0x001F, /*VDVFS14_CON1*/
  0x001F, /*VDVFS14_CON2*/
  0xC3C0, /*VDVFS14_CON3*/
  0x0378, /*VDVFS14_CON4*/
  0x0073, /*VDVFS14_CON5*/
  0x00FF, /*VDVFS14_CON6*/
  0x000F, /*VDVFS14_CON7*/
  0x3333, /*VDVFS14_CON8*/
  0xB031, /*VDVFS14_CON9*/
  0xFFFF, /*VDVFS14_CON10*/
  0x007F, /*VDVFS14_CON11*/
  0x007F, /*VDVFS14_CON12*/
  0x007F, /*VDVFS14_CON13*/
  0x007F, /*VDVFS14_CON14*/
  0xCDF3, /*VDVFS14_CON18*/
  0x3BE7, /*VDVFS14_CON19*/
  0x00FF, /*VDVFS14_CON20*/
  0x3F00, /*VGPU_CON0*/
  0x001F, /*VGPU_CON1*/
  0x001F, /*VGPU_CON2*/
  0xCFF7, /*VGPU_CON3*/
  0x0370, /*VGPU_CON4*/
  0x0073, /*VGPU_CON5*/
  0x00FF, /*VGPU_CON6*/
  0x000F, /*VGPU_CON7*/
  0x3333, /*VGPU_CON8*/
  0x8031, /*VGPU_CON9*/
  0xFFFF, /*VGPU_CON10*/
  0x007F, /*VGPU_CON11*/
  0x007F, /*VGPU_CON12*/
  0x007F, /*VGPU_CON13*/
  0x007F, /*VGPU_CON14*/
  0x7777, /*VGPU_CON15*/
  0x3333, /*VGPU_CON16*/
  0x3333, /*VGPU_CON17*/
  0xCDF3, /*VGPU_CON18*/
  0xE77F, /*VGPU_CON19*/
  0x009F, /*VGPU_CON20*/
  0x3F00, /*VCORE1_CON0*/
  0x001F, /*VCORE1_CON1*/
  0x001F, /*VCORE1_CON2*/
  0xCFF7, /*VCORE1_CON3*/
  0x037F, /*VCORE1_CON4*/
  0x0073, /*VCORE1_CON5*/
  0x00FF, /*VCORE1_CON6*/
  0x000F, /*VCORE1_CON7*/
  0x3333, /*VCORE1_CON8*/
  0x8031, /*VCORE1_CON9*/
  0xFFFF, /*VCORE1_CON10*/
  0x007F, /*VCORE1_CON11*/
  0x007F, /*VCORE1_CON12*/
  0x007F, /*VCORE1_CON13*/
  0x007F, /*VCORE1_CON14*/
  0x7777, /*VCORE1_CON15*/
  0x3333, /*VCORE1_CON16*/
  0x3333, /*VCORE1_CON17*/
  0xCDF3, /*VCORE1_CON18*/
  0xE77F, /*VCORE1_CON19*/
  0x009F, /*VCORE1_CON20*/
  0x3F00, /*VCORE2_CON0*/
  0x000F, /*VCORE2_CON1*/
  0x000F, /*VCORE2_CON2*/
  0xCFF7, /*VCORE2_CON3*/
  0x0370, /*VCORE2_CON4*/
  0x0073, /*VCORE2_CON5*/
  0x00FF, /*VCORE2_CON6*/
  0x000F, /*VCORE2_CON7*/
  0x3333, /*VCORE2_CON8*/
  0xB031, /*VCORE2_CON9*/
  0xFFFF, /*VCORE2_CON10*/
  0x007F, /*VCORE2_CON11*/
  0x007F, /*VCORE2_CON12*/
  0x007F, /*VCORE2_CON13*/
  0x007F, /*VCORE2_CON14*/
  0x7777, /*VCORE2_CON15*/
  0x3333, /*VCORE2_CON16*/
  0x3333, /*VCORE2_CON17*/
  0xCDF3, /*VCORE2_CON18*/
  0xE77F, /*VCORE2_CON19*/
  0x009F, /*VCORE2_CON20*/
  0x0080, /*VCORE2_CON21*/
  0x3F00, /*VIO18_CON0*/
  0x000F, /*VIO18_CON1*/
  0x000F, /*VIO18_CON2*/
  0xCFF7, /*VIO18_CON3*/
  0x0370, /*VIO18_CON4*/
  0x0033, /*VIO18_CON5*/
  0x00FF, /*VIO18_CON6*/
  0x000F, /*VIO18_CON7*/
  0x3333, /*VIO18_CON8*/
  0xB031, /*VIO18_CON9*/
  0xFFFF, /*VIO18_CON10*/
  0x007F, /*VIO18_CON11*/
  0x007F, /*VIO18_CON12*/
  0x007F, /*VIO18_CON13*/
  0x007F, /*VIO18_CON14*/
  0x7777, /*VIO18_CON15*/
  0x3333, /*VIO18_CON16*/
  0x3333, /*VIO18_CON17*/
  0xCDF3, /*VIO18_CON18*/
  0xE07F, /*VIO18_CON19*/
  0x009F, /*VIO18_CON20*/
  0x00FF, /*BUCK_K_CON0*/
  0x3F00, /*BUCK_K_CON1*/
  0x0000, /*BUCK_K_CON2*/
  0x03FF, /*BUCK_K_CON3*/
  0x07FF, /*ZCD_CON0*/
  0x0F9F, /*ZCD_CON1*/
  0x0F9F, /*ZCD_CON2*/
  0x001F, /*ZCD_CON3*/
  0x0707, /*ZCD_CON4*/
  0x3F3F, /*ZCD_CON5*/
  0xE0FC, /*ISINK0_CON0*/
  0xFFFF, /*ISINK0_CON1*/
  0x70FF, /*ISINK0_CON2*/
  0xFFFF, /*ISINK0_CON3*/
  0x0F0F, /*ISINK0_CON4*/
  0xE0FC, /*ISINK1_CON0*/
  0xFFFF, /*ISINK1_CON1*/
  0x70FF, /*ISINK1_CON2*/
  0xFFFF, /*ISINK1_CON3*/
  0x0F0F, /*ISINK1_CON4*/
  0xE0FC, /*ISINK2_CON0*/
  0xFFFF, /*ISINK2_CON1*/
  0x70FF, /*ISINK2_CON2*/
  0xFFFF, /*ISINK2_CON3*/
  0x0F0F, /*ISINK2_CON4*/
  0xE0FC, /*ISINK3_CON0*/
  0xFFFF, /*ISINK3_CON1*/
  0x70FF, /*ISINK3_CON2*/
  0xFFFF, /*ISINK3_CON3*/
  0x0F0F, /*ISINK3_CON4*/
  0xFFFF, /*ISINK_ANA0*/
  0x000F, /*ISINK_ANA1*/
  0x003F, /*ISINK_PHASE_DLY*/
  0x0FFF, /*ISINK_EN_CTRL*/
  0x0007, /*ANALDO_CON0*/
  0xEFE3, /*ANALDO_CON1*/
  0xEFE3, /*ANALDO_CON2*/
  0xEFE3, /*ANALDO_CON3*/
  0xEFE7, /*ANALDO_CON4*/
  0x8300, /*ANALDO_CON5*/
  0x0F64, /*ANALDO_CON6*/
  0x0F64, /*ANALDO_CON7*/
  0x0F64, /*ANALDO_CON8*/
  0x0F77, /*ANALDO_CON9*/
  0x0F67, /*ANALDO_CON10*/
  0xFE00, /*ANALDO_CON11*/
  0xF800, /*ANALDO_CON12*/
  0xFFFF, /*ANALDO_CON13*/
  0x01FF, /*SYSLDO_CON0*/
  0x83E3, /*SYSLDO_CON1*/
  0x8763, /*SYSLDO_CON2*/
  0x07E3, /*SYSLDO_CON3*/
  0xEFF3, /*SYSLDO_CON4*/
  0xEFE3, /*SYSLDO_CON5*/
  0x07E3, /*SYSLDO_CON6*/
  0x07E3, /*SYSLDO_CON7*/
  0x87E3, /*SYSLDO_CON8*/
  0x0F74, /*SYSLDO_CON9*/
  0x0F7C, /*SYSLDO_CON10*/
  0x0F7C, /*SYSLDO_CON11*/
  0xFFE4, /*SYSLDO_CON12*/
  0x0F7C, /*SYSLDO_CON13*/
  0x0F7C, /*SYSLDO_CON14*/
  0x0F7C, /*SYSLDO_CON15*/
  0x0F04, /*SYSLDO_CON16*/
  0x0387, /*SYSLDO_CON17*/
  0xFFC0, /*SYSLDO_CON18*/
  0x3FC0, /*SYSLDO_CON19*/
  0x0000, /*SYSLDO_CON20*/
  0xFFFF, /*SYSLDO_CON21*/
  0xFC7F, /*DIGLDO_CON0*/
  0x87E3, /*DIGLDO_CON1*/
  0x07E3, /*DIGLDO_CON2*/
  0xEFFB, /*DIGLDO_CON3*/
  0xEFE3, /*DIGLDO_CON4*/
  0xEFE3, /*DIGLDO_CON5*/
  0x07E3, /*DIGLDO_CON6*/
  0x07E3, /*DIGLDO_CON7*/
  0xEFE3, /*DIGLDO_CON8*/
  0xEFE3, /*DIGLDO_CON9*/
  0xE0E0, /*DIGLDO_CON10*/
  0x8101, /*DIGLDO_CON11*/
  0x07F3, /*DIGLDO_CON12*/
  0x0F74, /*DIGLDO_CON13*/
  0x0F74, /*DIGLDO_CON14*/
  0x0F74, /*DIGLDO_CON15*/
  0x0F44, /*DIGLDO_CON16*/
  0x0F44, /*DIGLDO_CON17*/
  0x0F74, /*DIGLDO_CON18*/
  0x0F74, /*DIGLDO_CON19*/
  0x0F74, /*DIGLDO_CON20*/
  0x0F74, /*DIGLDO_CON21*/
  0x0F74, /*DIGLDO_CON22*/
  0x0FFC, /*DIGLDO_CON23*/
  0xFFF0, /*DIGLDO_CON24*/
  0x3FF0, /*DIGLDO_CON25*/
  0x00FF, /*DIGLDO_CON26*/
  0xFFFF, /*DIGLDO_CON27*/
  0xFF80, /*DIGLDO_CON28*/
  0x003F, /*OTP_CON0*/
  0x00FF, /*OTP_CON1*/
  0x0003, /*OTP_CON2*/
  0x0001, /*OTP_CON3*/
  0x0001, /*OTP_CON4*/
  0x0001, /*OTP_CON5*/
  0xFFFF, /*OTP_CON6*/
  0xFFFF, /*OTP_CON7*/
  0x0001, /*OTP_CON8*/
  0x0001, /*OTP_CON9*/
  0x0001, /*OTP_CON10*/
  0x0001, /*OTP_CON11*/
  0xFFFF, /*OTP_CON12*/
  0x0000, /*OTP_CON13*/
  0x001F, /*OTP_CON14*/
  0x0000, /*OTP_DOUT_0_15*/
  0x0000, /*OTP_DOUT_16_31*/
  0x0000, /*OTP_DOUT_32_47*/
  0x0000, /*OTP_DOUT_48_63*/
  0x0000, /*OTP_DOUT_64_79*/
  0x0000, /*OTP_DOUT_80_95*/
  0x0000, /*OTP_DOUT_96_111*/
  0x0000, /*OTP_DOUT_112_127*/
  0x0000, /*OTP_DOUT_128_143*/
  0x0000, /*OTP_DOUT_144_159*/
  0x0000, /*OTP_DOUT_160_175*/
  0x0000, /*OTP_DOUT_176_191*/
  0x0000, /*OTP_DOUT_192_207*/
  0x0000, /*OTP_DOUT_208_223*/
  0x0000, /*OTP_DOUT_224_239*/
  0x0000, /*OTP_DOUT_240_255*/
  0xFFFF, /*OTP_VAL_0_15*/
  0xFFFF, /*OTP_VAL_16_31*/
  0xFFFF, /*OTP_VAL_32_47*/
  0xFFFF, /*OTP_VAL_48_63*/
  0xFFFF, /*OTP_VAL_64_79*/
  0xFFFF, /*OTP_VAL_80_95*/
  0xFFFF, /*OTP_VAL_96_111*/
  0xFFFF, /*OTP_VAL_112_127*/
  0xFFFF, /*OTP_VAL_128_143*/
  0xFFFF, /*OTP_VAL_144_159*/
  0xFFFF, /*OTP_VAL_160_175*/
  0xFFFF, /*OTP_VAL_176_191*/
  0xFFFF, /*OTP_VAL_192_207*/
  0xFFFF, /*OTP_VAL_208_223*/
  0xFFFF, /*OTP_VAL_224_239*/
  0xFFFF, /*OTP_VAL_240_255*/
  0x1FEB, /*RTC_MIX_CON0*/
  0x1BFF, /*RTC_MIX_CON1*/
  0x000F, /*AUDDAC_CFG0*/
  0xE1FF, /*AUDBUF_CFG0*/
  0x1FFF, /*AUDBUF_CFG1*/
  0x00FF, /*AUDBUF_CFG2*/
  0x1FFF, /*AUDBUF_CFG3*/
  0xFFFF, /*AUDBUF_CFG4*/
  0xFFFF, /*AUDBUF_CFG5*/
  0x7FFF, /*AUDBUF_CFG6*/
  0xFFCF, /*AUDBUF_CFG7*/
  0x7FFF, /*AUDBUF_CFG8*/
  0x03FF, /*IBIASDIST_CFG0*/
  0xFF1F, /*AUDCLKGEN_CFG0*/
  0xFF3F, /*AUDLDO_CFG0*/
  0x003F, /*AUDDCDC_CFG0*/
  0xFFFF, /*AUDDCDC_CFG1*/
  0x0001, /*AUDNVREGGLB_CFG0*/
  0xF000, /*AUD_NCP0*/
  0x000F, /*AUD_ZCD_CFG0*/
  0xFFFF, /*AUDPREAMP_CFG0*/
  0x1FFF, /*AUDPREAMP_CFG1*/
  0x07FF, /*AUDPREAMP_CFG2*/
  0xFFFF, /*AUDADC_CFG0*/
  0x7FFF, /*AUDADC_CFG1*/
  0xFFFF, /*AUDADC_CFG2*/
  0x3F3F, /*AUDADC_CFG3*/
  0x3F3F, /*AUDADC_CFG4*/
  0xFFFF, /*AUDADC_CFG5*/
  0x01FF, /*AUDDIGMI_CFG0*/
  0xF3FF, /*AUDDIGMI_CFG1*/
  0x3FFF, /*AUDMICBIAS_CFG0*/
  0x3FFF, /*AUDMICBIAS_CFG1*/
  0xFFFF, /*AUDENCSPARE_CFG0*/
  0x7777, /*AUDPREAMPGAIN_CFG0*/
  0x1FFF, /*AUDMADPLL_CFG0*/
  0x7FFF, /*AUDMADPLL_CFG1*/
  0xFFFF, /*AUDMADPLL_CFG2*/
  0xFFF8, /*AUDLDO_NVREG_CFG0*/
  0xFFFF, /*AUDLDO_NVREG_CFG1*/
  0xFFFF, /*AUDLDO_NVREG_CFG2*/
  0x8FFF, /*AUXADC_ADC0*/
  0x8FFF, /*AUXADC_ADC1*/
  0x8FFF, /*AUXADC_ADC2*/
  0x8FFF, /*AUXADC_ADC3*/
  0x8FFF, /*AUXADC_ADC4*/
  0x8FFF, /*AUXADC_ADC5*/
  0x8FFF, /*AUXADC_ADC6*/
  0xFFFF, /*AUXADC_ADC7*/
  0x8FFF, /*AUXADC_ADC8*/
  0x8FFF, /*AUXADC_ADC9*/
  0x8FFF, /*AUXADC_ADC10*/
  0x8FFF, /*AUXADC_ADC11*/
  0x0000, /*AUXADC_ADC12*/
  0x8FFF, /*AUXADC_ADC13*/
  0xFFFF, /*AUXADC_ADC14*/
  0xFFFF, /*AUXADC_ADC15*/
  0xFFFF, /*AUXADC_ADC16*/
  0x8FFF, /*AUXADC_ADC17*/
  0x8FFF, /*AUXADC_ADC18*/
  0x0000, /*AUXADC_ADC19*/
  0xFFFF, /*AUXADC_STA0*/
  0xF800, /*AUXADC_STA1*/
  0xFFFF, /*AUXADC_RQST0*/
  0x0000, /*AUXADC_RQST0_SET*/
  0x0000, /*AUXADC_RQST0_CLR*/
  0xFF9F, /*AUXADC_RQST1*/
  0x0000, /*AUXADC_RQST1_SET*/
  0x0000, /*AUXADC_RQST1_CLR*/
  0xFC3F, /*AUXADC_CON0*/
  0xFFFF, /*AUXADC_CON1*/
  0xFFFF, /*AUXADC_CON2*/
  0xFFFF, /*AUXADC_CON3*/
  0xC0FF, /*AUXADC_CON4*/
  0x7FFF, /*AUXADC_CON5*/
  0x7FFF, /*AUXADC_CON6*/
  0x09FF, /*AUXADC_CON7*/
  0x1FFF, /*AUXADC_CON8*/
  0xFFF0, /*AUXADC_CON9*/
  0xFFFF, /*AUXADC_CON10*/
  0xFFFF, /*AUXADC_CON11*/
  0xFFFF, /*AUXADC_CON12*/
  0xFFFF, /*AUXADC_CON13*/
  0xFFFF, /*AUXADC_CON14*/
  0xFFFF, /*AUXADC_CON15*/
  0x000F, /*AUXADC_CON16*/
  0xBFFF, /*AUXADC_CON17*/
  0xBFFF, /*AUXADC_CON18*/
  0x01FF, /*AUXADC_CON19*/
  0x01FF, /*AUXADC_CON20*/
  0xFFFF, /*AUXADC_CON21*/
  0xFFFF, /*AUXADC_CON22*/
  0xFFFF, /*AUXADC_CON23*/
  0x000F, /*AUXADC_CON24*/
  0xBFFF, /*AUXADC_CON25*/
  0xBFFF, /*AUXADC_CON26*/
  0x01FF, /*AUXADC_CON27*/
  0x01FF, /*AUXADC_CON28*/
  0xEFFF, /*ACCDET_CON0*/
  0x001F, /*ACCDET_CON1*/
  0x00FF, /*ACCDET_CON2*/
  0xFFFF, /*ACCDET_CON3*/
  0xFFFF, /*ACCDET_CON4*/
  0xFFFF, /*ACCDET_CON5*/
  0xFFFF, /*ACCDET_CON6*/
  0xFFFF, /*ACCDET_CON7*/
  0xFFFF, /*ACCDET_CON8*/
  0xFFFF, /*ACCDET_CON9*/
  0xFFFF, /*ACCDET_CON10*/
  0xC777, /*ACCDET_CON11*/
  0x8707, /*ACCDET_CON12*/
  0xF3FF, /*ACCDET_CON13*/
  0xF7FF, /*ACCDET_CON14*/
  0x3770, /*ACCDET_CON15*/
  0xFFFF, /*ACCDET_CON16*/
  0xEEEE, /*ACCDET_CON17*/
  0xFC00, /*ACCDET_CON18*/
  0x8707, /*ACCDET_CON19*/
  0xF01F, /*ACCDET_CON20*/
  0xFFFF, /*ACCDET_CON21*/
  0x7FFF, /*ACCDET_CON22*/
  0xFFFF, /*ACCDET_CON23*/
  0xFFFF, /*ACCDET_CON24*/
};

static kal_uint32 mt6331_aPMURegWr[16][2]= {  /* Register* , write val*/
  {0x063E, 0xFFF9}, /*OTP_VAL_0_15*/
  {0x0640, 0xFFFF}, /*OTP_VAL_16_31*/
  {0x0642, 0xFFFF}, /*OTP_VAL_32_47*/
  {0x0644, 0xF3FF}, /*OTP_VAL_48_63*/
  {0x0646, 0xC7FF}, /*OTP_VAL_64_79*/
  {0x0648, 0xFFFF}, /*OTP_VAL_80_95*/
  {0x064A, 0xFFFF}, /*OTP_VAL_96_111*/
  {0x064C, 0xFFFF}, /*OTP_VAL_112_127*/
  {0x064E, 0xFFFF}, /*OTP_VAL_128_143*/
  {0x0650, 0xFFFF}, /*OTP_VAL_144_159*/
  {0x0652, 0xFF9B}, /*OTP_VAL_160_175*/
  {0x0654, 0xFFFF}, /*OTP_VAL_176_191*/
  {0x0656, 0xFFFF}, /*OTP_VAL_192_207*/
  {0x0658, 0xFFFF}, /*OTP_VAL_208_223*/
  {0x065A, 0xFFFF}, /*OTP_VAL_224_239*/
  {0x065C, 0xFFF9}, /*OTP_VAL_240_255*/
};

static kal_uint32 mt6331_aPMURegWr_mask[16] = { /* mask*/
  0xFFFF, /*OTP_VAL_0_15*/
  0xFFFF, /*OTP_VAL_16_31*/
  0xFFFF, /*OTP_VAL_32_47*/
  0xFFFF, /*OTP_VAL_48_63*/
  0xFFFF, /*OTP_VAL_64_79*/
  0xFFFF, /*OTP_VAL_80_95*/
  0xFFFF, /*OTP_VAL_96_111*/
  0xFFFF, /*OTP_VAL_112_127*/
  0xFFFF, /*OTP_VAL_128_143*/
  0xFFFF, /*OTP_VAL_144_159*/
  0xFFFF, /*OTP_VAL_160_175*/
  0xFFFF, /*OTP_VAL_176_191*/
  0xFFFF, /*OTP_VAL_192_207*/
  0xFFFF, /*OTP_VAL_208_223*/
  0xFFFF, /*OTP_VAL_224_239*/
  0xFFFF, /*OTP_VAL_240_255*/
};





void top_6331_read(void)
{
    kal_uint32 u2PMUReg = 0;
    kal_uint32 u2Cnt = 0;
    kal_uint32 default_value_mask = 0;

    printk("RegNum,DefaultValue,Mask,GotValue,TestValue,Ans\n");

    for(u2Cnt = 0; u2Cnt < (sizeof(mt6331_aPMURegDef)/sizeof(*mt6331_aPMURegDef)); ++u2Cnt)
    {
       u2PMUReg = upmu_get_reg_value(    (mt6331_aPMURegDef[u2Cnt][0])  );

       //printk("[Before MASK] %x,%x,%x\r\n",(mt6331_aPMURegDef[u2Cnt][0]), u2PMUReg,(mt6331_aPMURegDef[u2Cnt][1]));
       //only check value of mask
       u2PMUReg &= mt6331_aPMURegDef_mask[u2Cnt];       
       //printk("[After MASK]%x,%x,%x\r\n",(mt6331_aPMURegDef[u2Cnt][0]), u2PMUReg,(mt6331_aPMURegDef[u2Cnt][1]));

       default_value_mask = ((mt6331_aPMURegDef[u2Cnt][1]) & mt6331_aPMURegDef_mask[u2Cnt]);      

       if(u2PMUReg != default_value_mask)
       {
           printk("[error] %x,%x,%x,%x,%x,%x\n",
            (mt6331_aPMURegDef[u2Cnt][0]), 
            (mt6331_aPMURegDef[u2Cnt][1]),
            mt6331_aPMURegDef_mask[u2Cnt],
            upmu_get_reg_value(    (mt6331_aPMURegDef[u2Cnt][0])  ),              
            u2PMUReg,            
            default_value_mask
            );
       }
    }        
}

void top_6331_write(int test_value)
{
    kal_uint32 u2PMUReg = 0;
    kal_uint32 u2Cnt = 0;
    kal_uint32 default_value_mask = 0;

    printk("RegNum,write_value(default_value_mask),Mask,GotValue,TestValue,Ans\n");

    for(u2Cnt = 0; u2Cnt < (sizeof(mt6331_aPMURegWr)/sizeof(*mt6331_aPMURegWr)); ++u2Cnt)
    {
       //write test value
       upmu_set_reg_value( (mt6331_aPMURegWr[u2Cnt][0]), test_value );
    
       //read back value 
       u2PMUReg = upmu_get_reg_value(    (mt6331_aPMURegWr[u2Cnt][0])  );

       //printk("[Before MASK] %x,%x,%x\r\n",(mt6331_aPMURegWr[u2Cnt][0]), u2PMUReg,(mt6331_aPMURegWr[u2Cnt][1]));       
       //only check value of mask
       u2PMUReg &= mt6331_aPMURegWr_mask[u2Cnt];
       //printk("[After MASK]%x,%x,%x\r\n",(mt6331_aPMURegWr[u2Cnt][0]), u2PMUReg,(mt6331_aPMURegWr[u2Cnt][1]));
       
       default_value_mask = (test_value & mt6331_aPMURegWr_mask[u2Cnt]);

       if(u2PMUReg != default_value_mask)
       {              
           printk("[error] %x,%x(%x),%x,%x,%x,%x\r\n",
            (mt6331_aPMURegWr[u2Cnt][0]),             
            test_value,
            default_value_mask,
            mt6331_aPMURegWr_mask[u2Cnt],
            upmu_get_reg_value(    (mt6331_aPMURegWr[u2Cnt][0])  ),              
            u2PMUReg, 
            default_value_mask
            );
       }
    }

    #if 0 //debug check
    for(u2Cnt = 0; u2Cnt < (sizeof(mt6331_aPMURegWr)/sizeof(*mt6331_aPMURegWr)); ++u2Cnt)
    {
        printk("Reg[%x] %x\n", 
            (mt6331_aPMURegWr[u2Cnt][0]), 
            upmu_get_reg_value(    (mt6331_aPMURegWr[u2Cnt][0])  )
            );
    }
    #endif
}

static kal_uint32 mt6332_aPMURegDef[597][2]= {  /* Register , reset val*/
  {0x8000, 0x3210},
  {0x8002, 0x3210},
  {0x8004, 0x0183},
  {0x8006, 0x007D},
  {0x8008, 0x007D},
  {0x800A, 0x007D},
  {0x800C, 0x0000},
  {0x800E, 0x0400},
  {0x8010, 0x0000},
  {0x8012, 0x0000},
  {0x8014, 0x0000},
  {0x8016, 0x0000},
  {0x8018, 0x0000},
  {0x801A, 0x0016},
  {0x801C, 0x0000},
  {0x801E, 0xCCCC},
  {0x8020, 0x000C},
  {0x8022, 0xCCCC},
  {0x8024, 0x0000},
  {0x8026, 0x0000},
  {0x8028, 0x0000},
  {0x802A, 0x0000},
  {0x802C, 0x0000},
  {0x802E, 0x1000},
  {0x8030, 0x1000},
  {0x8032, 0x000F},
  {0x8034, 0x0000},
  {0x8036, 0x0001},
  {0x8038, 0x0000},
  {0x803A, 0x4000},
  {0x803C, 0x000B},
  {0x803E, 0x0300},
  {0x8040, 0x0000},
  {0x8042, 0x4000},
  {0x8044, 0x0000},
  {0x8046, 0x0000},
  {0x8048, 0x004F},
  {0x804A, 0x0000},
  {0x804C, 0x0000},
  {0x804E, 0x0000},
  {0x8050, 0x0000},
  {0x8052, 0x0000},
  {0x8054, 0x0000},
  {0x8056, 0x0000},
  {0x8058, 0x0000},
  {0x805A, 0x0000},
  {0x805C, 0x0000},
  {0x805E, 0x0000},
  {0x8060, 0x0000},
  {0x8062, 0x0000},
  {0x8064, 0x0000},
  {0x8066, 0x0100},
  {0x8068, 0x001F},
  {0x806A, 0xC000},
  {0x806C, 0xFFFF},
  {0x806E, 0x0000},
  {0x8070, 0x1000},
  {0x8072, 0x0000},
  {0x8074, 0x0000},
  {0x8076, 0x0000},
  {0x8078, 0x0000},
  {0x807A, 0x0000},
  {0x807C, 0x0000},
  {0x807E, 0xFFE0},
  {0x8080, 0x0100},
  {0x8082, 0x0001},
  {0x8084, 0x0000},
  {0x8086, 0xF900},
  {0x8088, 0x0000},
  {0x808A, 0x0000},
  {0x808C, 0x0000},
  {0x808E, 0x0000},
  {0x8090, 0x0000},
  {0x8092, 0x0000},
  {0x8094, 0x74FE},
  {0x8096, 0x0000},
  {0x8098, 0x0000},
  {0x809A, 0x6027},
  {0x809C, 0x0000},
  {0x809E, 0x0000},
  {0x80A0, 0x004A},
  {0x80A2, 0x0000},
  {0x80A4, 0x0000},
  {0x80A6, 0x0100},
  {0x80A8, 0x0000},
  {0x80AA, 0x0000},
  {0x80AC, 0x0000},
  {0x80AE, 0x0000},
  {0x80B0, 0x0000},
  {0x80B2, 0x01FF},
  {0x80B4, 0x0000},
  {0x80B6, 0x0000},
  {0x80B8, 0x0000},
  {0x80BA, 0x0000},
  {0x80BC, 0x0000},
  {0x80BE, 0x0000},
  {0x80C0, 0x0000},
  {0x80C2, 0x0000},
  {0x80C4, 0x0000},
  {0x80C6, 0x0000},
  {0x80C8, 0x3FFF},
  {0x80CA, 0x0000},
  {0x80CC, 0x0000},
  {0x80CE, 0x00FF},
  {0x80D0, 0x0000},
  {0x80D2, 0x0000},
  {0x80D4, 0x0000},
  {0x80D6, 0x0000},
  {0x80D8, 0x0000},
  {0x80DA, 0x0000},
  {0x80DC, 0x0000},
  {0x80DE, 0x0000},
  {0x80E0, 0x000D},
  {0x80E2, 0x0000},
  {0x80E4, 0x0000},
  {0x80E6, 0x0000},
  {0x80E8, 0x0000},
  {0x80EA, 0x0000},
  {0x80EC, 0x0000},
  {0x80EE, 0x0000},
  {0x80F0, 0x0001},
  {0x80F2, 0x0000},
  {0x80F4, 0x0000},
  {0x80F6, 0x0000},
  {0x80F8, 0xA55A},
  {0x80FA, 0x0000},
  {0x80FC, 0x0000},
  {0x80FE, 0x0000},
  {0x8100, 0x0083},
  {0x8102, 0x0000},
  {0x8104, 0x0000},
  {0x8106, 0x0000},
  {0x8108, 0x0000},
  {0x810A, 0x0000},
  {0x810C, 0x0000},
  {0x810E, 0x0000},
  {0x8110, 0x000F},
  {0x8112, 0x0000},
  {0x8114, 0x0000},
  {0x8116, 0x0000},
  {0x8118, 0x0000},
  {0x811A, 0x0000},
  {0x811C, 0x0000},
  {0x811E, 0x0000},
  {0x8120, 0x0000},
  {0x8122, 0x0000},
  {0x8124, 0x0000},
  {0x8126, 0x0000},
  {0x8128, 0x0000},
  {0x812A, 0x0000},
  {0x812C, 0x0000},
  {0x812E, 0x0000},
  {0x8130, 0x0000},
  {0x8132, 0x400A},
  {0x8134, 0x1431},
  {0x8136, 0x1011},
  {0x8138, 0x0000},
  {0x813A, 0x0000},
  {0x813C, 0x0100},
  {0x813E, 0x0100},
  {0x8140, 0x0100},
  {0x8142, 0x0100},
  {0x8144, 0x0100},
  {0x8146, 0x0100},
  {0x8148, 0x0100},
  {0x814A, 0x0100},
  {0x814C, 0x0100},
  {0x814E, 0x0100},
  {0x8150, 0x0000},
  {0x8152, 0x0000},
  {0x8154, 0x0010},
  {0x8156, 0x4000},
  {0x8158, 0x0000},
  {0x815A, 0x0000},
  {0x815C, 0x0000},
  {0x815E, 0x5002},
  {0x8160, 0x0000},
  {0x8400, 0x0000},
  {0x8402, 0xB100},
  {0x8404, 0x110A},
  {0x8406, 0xC10B},
  {0x8408, 0x0000},
  {0x840A, 0x0000},
  {0x840C, 0x0000},
  {0x840E, 0x0000},
  {0x8410, 0x000A},
  {0x8412, 0x000A},
  {0x8414, 0x000A},
  {0x8416, 0x000A},
  {0x8418, 0x000A},
  {0x841A, 0x000A},
  {0x841C, 0x0000},
  {0x841E, 0x0000},
  {0x8420, 0x003F},
  {0x8422, 0x0000},
  {0x8424, 0x0000},
  {0x8426, 0x0000},
  {0x8428, 0x0000},
  {0x842A, 0x0000},
  {0x842C, 0x0000},
  {0x842E, 0x0000},
  {0x8430, 0x0000},
  {0x8432, 0x0000},
  {0x8434, 0x0000},
  {0x8436, 0x0000},
  {0x8438, 0x0000},
  {0x843A, 0x0000},
  {0x843C, 0x0000},
  {0x843E, 0x0746},
  {0x8440, 0x0200},
  {0x8442, 0x0011},
  {0x8444, 0x00F0},
  {0x8446, 0x0000},
  {0x8448, 0x0000},
  {0x844A, 0x3021},
  {0x844C, 0x0808},
  {0x844E, 0x0068},
  {0x8450, 0x0038},
  {0x8452, 0x0038},
  {0x8454, 0x0068},
  {0x8456, 0x3333},
  {0x8458, 0x3333},
  {0x845A, 0x3333},
  {0x845C, 0x0001},
  {0x845E, 0x0016},
  {0x8460, 0x0080},
  {0x8462, 0x0000},
  {0x8464, 0x0000},
  {0x8466, 0x0000},
  {0x8468, 0x0000},
  {0x846A, 0x0746},
  {0x846C, 0x0200},
  {0x846E, 0x0001},
  {0x8470, 0x00F0},
  {0x8472, 0x0000},
  {0x8474, 0x0000},
  {0x8476, 0x3021},
  {0x8478, 0x0808},
  {0x847A, 0x0030},
  {0x847C, 0x0030},
  {0x847E, 0x0030},
  {0x8480, 0x0030},
  {0x8482, 0x3333},
  {0x8484, 0x3333},
  {0x8486, 0x3333},
  {0x8488, 0x0001},
  {0x848A, 0x0016},
  {0x848C, 0x0080},
  {0x848E, 0x0000},
  {0x8490, 0x0000},
  {0x8492, 0x0050},
  {0x8494, 0x0050},
  {0x8496, 0x0808},
  {0x8498, 0x0001},
  {0x849A, 0x0000},
  {0x849C, 0x0000},
  {0x849E, 0x0000},
  {0x84A0, 0x0000},
  {0x84A2, 0x0885},
  {0x84A4, 0x0200},
  {0x84A6, 0x0033},
  {0x84A8, 0x00F0},
  {0x84AA, 0x0000},
  {0x84AC, 0x0000},
  {0x84AE, 0x0020},
  {0x84B0, 0x0808},
  {0x84B2, 0x0038},
  {0x84B4, 0x0038},
  {0x84B6, 0x0038},
  {0x84B8, 0x0038},
  {0x84BA, 0x2222},
  {0x84BC, 0x3333},
  {0x84BE, 0x3333},
  {0x84C0, 0x0001},
  {0x84C2, 0x000F},
  {0x84C4, 0x0080},
  {0x84C6, 0x0080},
  {0x84C8, 0x0000},
  {0x84CA, 0x0000},
  {0x84CC, 0x0000},
  {0x84CE, 0x0885},
  {0x84D0, 0x0200},
  {0x84D2, 0x0033},
  {0x84D4, 0x00F0},
  {0x84D6, 0x0000},
  {0x84D8, 0x0000},
  {0x84DA, 0x0020},
  {0x84DC, 0x0808},
  {0x84DE, 0x0038},
  {0x84E0, 0x0038},
  {0x84E2, 0x0038},
  {0x84E4, 0x0038},
  {0x84E6, 0x2222},
  {0x84E8, 0x3333},
  {0x84EA, 0x3333},
  {0x84EC, 0x0001},
  {0x84EE, 0x000F},
  {0x84F0, 0x0080},
  {0x84F2, 0x0080},
  {0x84F4, 0x3F00},
  {0x84F6, 0x0000},
  {0x84F8, 0x0000},
  {0x84FC, 0x0000},
  {0x84FE, 0x0200},
  {0x8500, 0x0000},
  {0x8502, 0x0000},
  {0x8504, 0x0000},
  {0x8506, 0x0000},
  {0x8508, 0x0020},
  {0x850A, 0x0808},
  {0x850C, 0x0000},
  {0x850E, 0x0000},
  {0x8510, 0x0000},
  {0x8512, 0x0000},
  {0x8514, 0x0000},
  {0x8516, 0x0000},
  {0x8518, 0xFF00},
  {0x851A, 0xCC01},
  {0x851C, 0x0000},
  {0x851E, 0x0000},
  {0x8520, 0x2E14},
  {0x8522, 0x0E01},
  {0x8524, 0x0000},
  {0x8526, 0xF001},
  {0x8528, 0x0000},
  {0x852A, 0x0000},
  {0x852C, 0x0000},
  {0x852E, 0x0000},
  {0x8530, 0x0000},
  {0x8532, 0x01C0},
  {0x8534, 0x0200},
  {0x8536, 0x0000},
  {0x8538, 0x0000},
  {0x853A, 0x0000},
  {0x853C, 0x0300},
  {0x853E, 0x0020},
  {0x8540, 0x0808},
  {0x8542, 0x0040},
  {0x8544, 0x0040},
  {0x8546, 0x0040},
  {0x8548, 0x0040},
  {0x854A, 0x0000},
  {0x854C, 0x0000},
  {0x854E, 0x0000},
  {0x8550, 0x0000},
  {0x8552, 0x0000},
  {0x8554, 0x0009},
  {0x8556, 0x0000},
  {0x8558, 0x0000},
  {0x855A, 0x0000},
  {0x855C, 0x0000},
  {0x855E, 0x016F},
  {0x8560, 0x004D},
  {0x8800, 0x0000},
  {0x8802, 0x0000},
  {0x8804, 0x0000},
  {0x8806, 0x0000},
  {0x8808, 0x0000},
  {0x880A, 0x0000},
  {0x880C, 0x0000},
  {0x880E, 0x0000},
  {0x8810, 0x0000},
  {0x8812, 0x0000},
  {0x8814, 0x0000},
  {0x8816, 0x0000},
  {0x8818, 0x0000},
  {0x881A, 0x0000},
  {0x881C, 0x0000},
  {0x881E, 0x0000},
  {0x8820, 0x0000},
  {0x8822, 0x0000},
  {0x8824, 0x0000},
  {0x8826, 0x0000},
  {0x8828, 0x0000},
  {0x882A, 0x0000},
  {0x882C, 0x0000},
  {0x882E, 0x0000},
  {0x8830, 0x0000},
  {0x8832, 0x0000},
  {0x8834, 0x0000},
  {0x8836, 0x0000},
  {0x8838, 0x0000},
  {0x883A, 0x0000},
  {0x883C, 0x0000},
  {0x883E, 0x0000},
  {0x8840, 0x0000},
  {0x8842, 0x0000},
  {0x8844, 0x0000},
  {0x8846, 0x0000},
  {0x8848, 0x0000},
  {0x884A, 0x0000},
  {0x884C, 0x0000},
  {0x884E, 0x0000},
  {0x8850, 0x0000},
  {0x8852, 0x0000},
  {0x8854, 0x0000},
  {0x8856, 0x0000},
  {0x8858, 0x0000},
  {0x885A, 0x0000},
  {0x885C, 0x0000},
  {0x885E, 0x0000},
  {0x8860, 0x0000},
  {0x8862, 0x0000},
  {0x8864, 0x0000},
  {0x8866, 0x0000},
  {0x8868, 0x8014},
  {0x886A, 0x00B0},
  {0x886C, 0x0080},
  {0x886E, 0x2040},
  {0x8870, 0x4000},
  {0x8872, 0x0000},
  {0x8874, 0x0000},
  {0x8876, 0x0000},
  {0x8878, 0x0000},
  {0x887A, 0x0000},
  {0x887C, 0x0000},
  {0x887E, 0x01E2},
  {0x8880, 0x0015},
  {0x8882, 0x0020},
  {0x8884, 0x0000},
  {0x8886, 0x0000},
  {0x8888, 0x0000},
  {0x888A, 0x8000},
  {0x888C, 0x8000},
  {0x888E, 0x0000},
  {0x8890, 0x0000},
  {0x8892, 0x00FF},
  {0x8894, 0x0000},
  {0x8896, 0x0000},
  {0x8898, 0x0000},
  {0x889A, 0x8000},
  {0x889C, 0x8000},
  {0x889E, 0x0000},
  {0x88A0, 0x0000},
  {0x88AA, 0x0000},
  {0x88AC, 0x0000},
  {0x88AE, 0x0000},
  {0x88B0, 0x00BC},
  {0x8C00, 0x0000},
  {0x8C02, 0x0000},
  {0x8C04, 0x0000},
  {0x8C06, 0x0040},
  {0x8C08, 0x0000},
  {0x8C0A, 0x4001},
  {0x8C0C, 0x0000},
  {0x8C0E, 0x0000},
  {0x8C10, 0x0000},
  {0x8C12, 0x0000},
  {0x8C14, 0x0000},
  {0x8C16, 0x0000},
  {0x8C18, 0x0020},
  {0x8C1A, 0x0000},
  {0x8C1C, 0x0000},
  {0x8C1E, 0x0000},
  {0x8C20, 0x0800},
  {0x8C22, 0x0000},
  {0x8C24, 0x0000},
  {0x8C26, 0x0000},
  {0x8C28, 0x0020},
  {0x8C2A, 0x0000},
  {0x8C2C, 0x0000},
  {0x8C2E, 0x0000},
  {0x8C30, 0x0000},
  {0x8C32, 0x0000},
  {0x8C34, 0x0000},
  {0x8C36, 0x0000},
  {0x8C38, 0x0000},
  {0x8C3A, 0x0000},
  {0x8C3C, 0x0000},
  {0x8C3E, 0x000B},
  {0x8C40, 0x0000},
  {0x8C42, 0x0000},
  {0x8C44, 0x0000},
  {0x8C46, 0x0000},
  {0x8C48, 0x0000},
  {0x8C4A, 0x0000},
  {0x8C4C, 0x0000},
  {0x8C4E, 0x0000},
  {0x8C50, 0x0000},
  {0x8C52, 0x0000},
  {0x8C54, 0x0000},
  {0x8C56, 0x0000},
  {0x8C58, 0x0000},
  {0x8C5A, 0x0000},
  {0x8C5C, 0x0000},
  {0x8C5E, 0x0000},
  {0x8C60, 0x0000},
  {0x8C62, 0x0000},
  {0x8C64, 0x0000},
  {0x8C66, 0x0000},
  {0x8C68, 0x0000},
  {0x8C6A, 0x0000},
  {0x8C6C, 0x0000},
  {0x8C6E, 0x0000},
  {0x8C70, 0x0000},
  {0x8C72, 0x0000},
  {0x8C74, 0x0000},
  {0x8C76, 0x0000},
  {0x8C78, 0x0000},
  {0x8C7A, 0x0000},
  {0x8C7C, 0x0000},
  {0x8C7E, 0x0000},
  {0x8C80, 0x0000},
  {0x8C82, 0x0000},
  {0x8C84, 0x0000},
  {0x8C86, 0x0000},
  {0x8C88, 0x0000},
  {0x8C8A, 0x0000},
  {0x8C8C, 0x0000},
  {0x8C8E, 0x0000},
  {0x8C90, 0x0000},
  {0x8C92, 0x0000},
  {0x8C94, 0x000E},
  {0x8C96, 0x0003},
  {0x8C98, 0x0000},
  {0x8C9A, 0x0032},
  {0x8C9C, 0x0000},
  {0x8C9E, 0x0000},
  {0x8CA0, 0x0000},
  {0x8CA2, 0x0000},
  {0x8CA4, 0xE004},
  {0x8CA6, 0x0009},
  {0x8CA8, 0x0000},
  {0x8CAA, 0x0000},
  {0x8CAC, 0x0000},
  {0x8CAE, 0x000C},
  {0x8CB0, 0x0000},
  {0x8CB2, 0x0000},
  {0x8CB4, 0x0000},
  {0x8CB6, 0xC540},
  {0x8CB8, 0x4140},
  {0x8CBA, 0x8540},
  {0x8CBC, 0x8500},
  {0x8CBE, 0x0004},
  {0x8CC0, 0x0004},
  {0x8CC2, 0x6484},
  {0x8CC4, 0x0064},
  {0x8CC6, 0x0000},
  {0x8CC8, 0x0000},
  {0x8CCA, 0x6300},
  {0x8CCC, 0x0000},
  {0x8CCE, 0x0000},
  {0x8CD0, 0x0000},
  {0x8CD2, 0x0000},
  {0x8CD4, 0x0574},
  {0x8CD6, 0x0000},
  {0x8CD8, 0x0000},
  {0x8CDA, 0x0000},
  {0x8CDC, 0x0000},
  {0x8CDE, 0x8000},
  {0x8CE0, 0x0000},
  {0x8CE2, 0x0000},
  {0x8CE4, 0x03FF},
  {0x8CE6, 0x0000},
  {0x8CE8, 0x0800},
  {0x8CEA, 0x0001},
  {0x8CEC, 0x0000},
  {0x8CEE, 0x01F4},
  {0x8CF0, 0x0000},
  {0x8CF2, 0x0000},
  {0x8CF4, 0x0000},
  {0x8CF6, 0x0094},
  {0x8CF8, 0x0000},
  {0x8CFA, 0x0000},
  {0x8CFC, 0x0094},
  {0x8CFE, 0x0000},
  {0x8D00, 0x4531},
  {0x8D02, 0x0000},
  {0x8D04, 0x2000},
  {0x8D06, 0x0000},
  {0x8D08, 0x0000},
  {0x8D0A, 0x0000},
  {0x8D0C, 0x0000},
  {0x8D0E, 0x0000},
  {0x8D10, 0x0000},
  {0x8D12, 0x0000},
  {0x8D14, 0x0000},
  {0x8D16, 0x0000},
  {0x8D18, 0x0000},
  {0x8D1A, 0x0000},
  {0x8D1C, 0x0000},
  {0x8D1E, 0x0000},
  {0x8D20, 0x0000},
  {0x8D22, 0x0000},
  {0x8D24, 0x0000},
  {0x8D26, 0x0000},
  {0x8D28, 0x0000},
  {0x8D2A, 0x0000},
  {0x8D2C, 0x0000},
  {0x8D2E, 0x0000},
  {0x8D30, 0x0000},
  {0x8D32, 0x0000},
  {0x8D34, 0x0000},
  {0x8D36, 0x0000},
  {0x8D38, 0x0000},
  {0x8D3A, 0x0000},
  {0x8D3C, 0x0000}
};

static kal_uint32 mt6332_aPMURegDef_mask[597] = { /* mask*/
  0xFFFF,
  0xFFFF,
  0x03FF,
  0x00FF,
  0x00FF,
  0x00FF,
  0x0000,
  0x0FFF,
  0x01BF,
  0x0001,
  0x001F,
  0x0003,
  0x0003,
  0x003F,
  0x000F,
  0xFFFF,
  0x000F,
  0xFFFF,
  0x0000,
  0x0000,
  0x000F,
  0x0000,
  0x0000,
  0x3FFF,
  0x3FFF,
  0x000F,
  0x0FFF,
  0x7FFF,
  0x7FFF,
  0xFFFF,
  0x7FFF,
  0x7FFF,
  0x3FFF,
  0x7FFF,
  0x3FFF,
  0xFFFF,
  0x01FF,
  0x03FF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0x000F,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0001,
  0x0101,
  0x3B5F,
  0xDB2D,
  0xFFFF,
  0x76DB,
  0xFF7D,
  0x00FB,
  0x002D,
  0x0005,
  0x0B55,
  0xFFEB,
  0xFF15,
  0xFFED,
  0x0175,
  0x0101,
  0x0005,
  0xFD57,
  0xFF77,
  0x2F2F,
  0x06DB,
  0xFF7D,
  0xFF7D,
  0xFFFF,
  0x7FFF,
  0x0000,
  0x0000,
  0xF3FF,
  0x0000,
  0x0000,
  0xFFFF,
  0x0000,
  0x0000,
  0xF70F,
  0x0000,
  0x0000,
  0xFF33,
  0x0000,
  0x0000,
  0x03FF,
  0x0000,
  0x0000,
  0x01FF,
  0x03FF,
  0xFFFF,
  0x0000,
  0x0000,
  0x001B,
  0x0000,
  0x0000,
  0x3FFF,
  0x0000,
  0x0000,
  0x00FF,
  0x0000,
  0x0000,
  0x07FF,
  0x0000,
  0x0000,
  0x007F,
  0x0000,
  0x0000,
  0x008F,
  0x0001,
  0x3FFF,
  0x00FF,
  0x07FF,
  0x007F,
  0x0003,
  0x0003,
  0x0003,
  0x0007,
  0x0001,
  0x0001,
  0xFFFF,
  0xFFFF,
  0x0001,
  0x0001,
  0x00FF,
  0x000F,
  0x0003,
  0x0003,
  0x0001,
  0x0001,
  0x0001,
  0x0001,
  0x000F,
  0x0003,
  0x07FF,
  0x07FF,
  0x07FF,
  0x07FF,
  0x07FF,
  0x07FF,
  0x07FF,
  0x07FF,
  0x07FF,
  0x07FF,
  0x07FF,
  0x07FF,
  0x07FF,
  0x07FF,
  0x07FF,
  0xF37F,
  0xFDFF,
  0xF01F,
  0x0001,
  0x100F,
  0x81FF,
  0x81FF,
  0x81FF,
  0x81FF,
  0x81FF,
  0x81FF,
  0x81FF,
  0x81FF,
  0x81FF,
  0x81FF,
  0xF9FF,
  0xF803,
  0x801F,
  0xEFF8,
  0x0000,
  0xFFFF,
  0x3FFF,
  0xFF1F,
  0xFFFF,
  0xFF80,
  0xF1FF,
  0xF1FF,
  0xF1FF,
  0xFFFF,
  0x0000,
  0x0000,
  0x0000,
  0x00CF,
  0x00CF,
  0x00CF,
  0x00CF,
  0x00CF,
  0x00CF,
  0x003F,
  0x3F3F,
  0x003F,
  0x003F,
  0x0007,
  0x7F7F,
  0x7F7F,
  0x007F,
  0xF000,
  0x000F,
  0xF000,
  0x000F,
  0xF800,
  0x001F,
  0x3F00,
  0x000F,
  0x000F,
  0xCFF7,
  0x0370,
  0x0033,
  0x00FF,
  0x000F,
  0x3333,
  0xB031,
  0xFFFF,
  0x007F,
  0x007F,
  0x007F,
  0x007F,
  0x7777,
  0x3333,
  0x3333,
  0xCDF3,
  0xE47F,
  0x009F,
  0x00E0,
  0x3F00,
  0x001F,
  0x001F,
  0xCFF7,
  0x0370,
  0x0033,
  0x00FF,
  0x000F,
  0x3333,
  0xB031,
  0xFFFF,
  0x007F,
  0x007F,
  0x007F,
  0x007F,
  0x7777,
  0x3333,
  0x3333,
  0xCDF3,
  0xE47F,
  0x009F,
  0x00E7,
  0x0007,
  0x007F,
  0x007F,
  0xFFFF,
  0x0DF3,
  0x0030,
  0x3F00,
  0x000F,
  0x000F,
  0xCFF7,
  0x0370,
  0x0033,
  0x00FF,
  0x000F,
  0x3333,
  0xB031,
  0xFFFF,
  0x007F,
  0x007F,
  0x007F,
  0x007F,
  0x7777,
  0x3333,
  0x3333,
  0xCDF3,
  0xE47F,
  0x009F,
  0x0080,
  0x3F00,
  0x000F,
  0x000F,
  0xCFF7,
  0x0370,
  0x0033,
  0x00FF,
  0x000F,
  0x3333,
  0xB031,
  0xFFFF,
  0x007F,
  0x007F,
  0x007F,
  0x007F,
  0x7777,
  0x3333,
  0x3333,
  0xCDF3,
  0xE47F,
  0x009F,
  0x0080,
  0x3F00,
  0x001F,
  0xF01F,
  0x3FF3,
  0xF300,
  0x003F,
  0xFFFF,
  0x000F,
  0x3333,
  0xB031,
  0xFFFF,
  0x003F,
  0x003F,
  0x003F,
  0x003F,
  0x3800,
  0x7777,
  0xFFFF,
  0xCCF3,
  0x3333,
  0x3333,
  0x3F3F,
  0x3F01,
  0x00FB,
  0xFFF3,
  0x001F,
  0x3F00,
  0x001F,
  0x001F,
  0xF000,
  0x3FFE,
  0x0370,
  0x003F,
  0x00FF,
  0x000F,
  0x3333,
  0xB031,
  0xFFFF,
  0x007F,
  0x007F,
  0x007F,
  0x007F,
  0x3333,
  0x3333,
  0x01FB,
  0x009F,
  0x00A5,
  0x001B,
  0x00FF,
  0x00FC,
  0x0000,
  0x0000,
  0x03FF,
  0x03FF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0xFFFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0x8FFF,
  0x0000,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x8FFF,
  0x0000,
  0xFFFF,
  0xF800,
  0xFFFF,
  0x0000,
  0x0000,
  0xFF9F,
  0x0000,
  0x0000,
  0xFC3F,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xC0FF,
  0x7FFF,
  0x7FFF,
  0x09FF,
  0x1FFF,
  0xFFF0,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0x000F,
  0xBFFF,
  0xBFFF,
  0x01FF,
  0x01FF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0x000F,
  0xBFFF,
  0xBFFF,
  0x01FF,
  0x01FF,
  0xBF3F,
  0xBF3F,
  0xBF3F,
  0xFFFF,
  0x00FF,
  0x00FF,
  0x00FF,
  0x007F,
  0x001F,
  0x7037,
  0x000F,
  0x0003,
  0xBFFF,
  0xFFFE,
  0x0030,
  0x7F03,
  0x00F0,
  0x0003,
  0xFFFF,
  0xFFE7,
  0xF80F,
  0x00FF,
  0xFFFF,
  0x8001,
  0xFF7D,
  0xFFFF,
  0xFFFF,
  0x3FFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFF1,
  0x030F,
  0x71B3,
  0xFFFF,
  0xFFFF,
  0x00FF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0x3FFF,
  0xFFFF,
  0xFFFF,
  0xC0FF,
  0x003F,
  0x00FF,
  0x0003,
  0x0001,
  0x0001,
  0x0001,
  0xFFFF,
  0xFFFF,
  0x0001,
  0x0001,
  0x0001,
  0x0001,
  0xFFFF,
  0x0000,
  0x001F,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0x01FF,
  0xEF67,
  0xEF63,
  0x8763,
  0xEF67,
  0x0F04,
  0x0F04,
  0xFFE4,
  0x0F64,
  0xFA00,
  0xF000,
  0xFF80,
  0xFFFF,
  0x800F,
  0xFFFF,
  0xFFFF,
  0x3FFF,
  0xE03F,
  0xE00C,
  0x0003,
  0xFFFF,
  0xF0FF,
  0xFF00,
  0x8FF0,
  0xEFFF,
  0xF01F,
  0xFFFF,
  0x3FFF,
  0x3FFF,
  0x3FFF,
  0xFFFF,
  0x370D,
  0xFF80,
  0x07FF,
  0x330D,
  0xFF80,
  0x7FFF,
  0xFF37,
  0x7FFF,
  0x7FFF,
  0xFF00,
  0x0077,
  0xFFFF,
  0xFF80,
  0xFFFF,
  0xFFF0,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0x00FF,
  0x00FF,
  0x0007
};

//static kal_uint32 mt6332_aPMURegWr[16][2]= {  /* Register* , write val*/
  static kal_uint32 mt6332_aPMURegWr[15][2]= {  /* Register* , write val*/
  {0x8C94, 0xFFF1},
  {0x8C96, 0xFFFC},
  //{0x8C98, 0xFFFF},
  {0x8C9A, 0xFFCD},
  {0x8C9C, 0xFFFF},
  {0x8C9E, 0xFFFF},
  {0x8CA0, 0xFFFF},
  {0x8CA2, 0xFFFF},
  {0x8CA4, 0x1FFB},
  {0x8CA6, 0xFFF6},
  {0x8CA8, 0xFFFF},
  {0x8CAA, 0xFFFF},
  {0x8CAC, 0xFFFF},
  {0x8CAE, 0xFFF3},
  {0x8CB0, 0xFFFF},
  {0x8CB2, 0xFFFF}
};

//static kal_uint32 mt6332_aPMURegWr_mask[16] = { /* mask*/
static kal_uint32 mt6332_aPMURegWr_mask[15] = { /* mask*/
  0xFFFF,
  0xFFFF,
  //0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF,
  0xFFFF
};



void top_6332_read(void)
{
    kal_uint32 u2PMUReg = 0;
    kal_uint32 u2Cnt = 0;
    kal_uint32 default_value_mask = 0;

    printk("RegNum,DefaultValue,Mask,GotValue,TestValue,Ans\n");

    for(u2Cnt = 0; u2Cnt < (sizeof(mt6332_aPMURegDef)/sizeof(*mt6332_aPMURegDef)); ++u2Cnt)
    {
       u2PMUReg = upmu_get_reg_value(    (mt6332_aPMURegDef[u2Cnt][0])  );

       //printk("[Before MASK] %x,%x,%x\r\n",(mt6332_aPMURegDef[u2Cnt][0]), u2PMUReg,(mt6332_aPMURegDef[u2Cnt][1]));
       //only check value of mask
       u2PMUReg &= mt6332_aPMURegDef_mask[u2Cnt];       
       //printk("[After MASK]%x,%x,%x\r\n",(mt6332_aPMURegDef[u2Cnt][0]), u2PMUReg,(mt6332_aPMURegDef[u2Cnt][1]));

       default_value_mask = ((mt6332_aPMURegDef[u2Cnt][1]) & mt6332_aPMURegDef_mask[u2Cnt]);      

       if(u2PMUReg != default_value_mask)
       {
           printk("[error] %x,%x,%x,%x,%x,%x\n",
            (mt6332_aPMURegDef[u2Cnt][0]), 
            (mt6332_aPMURegDef[u2Cnt][1]),
            mt6332_aPMURegDef_mask[u2Cnt],
            upmu_get_reg_value(    (mt6332_aPMURegDef[u2Cnt][0])  ),              
            u2PMUReg,            
            default_value_mask
            );
       }
    }        
}

void top_6332_write(int test_value)
{
    kal_uint32 u2PMUReg = 0;
    kal_uint32 u2Cnt = 0;
    kal_uint32 default_value_mask = 0;

    printk("RegNum,write_value(default_value_mask),Mask,GotValue,TestValue,Ans\n");

    for(u2Cnt = 0; u2Cnt < (sizeof(mt6332_aPMURegWr)/sizeof(*mt6332_aPMURegWr)); ++u2Cnt)
    {
       //write test value
       upmu_set_reg_value( (mt6332_aPMURegWr[u2Cnt][0]), test_value );
    
       //read back value 
       u2PMUReg = upmu_get_reg_value(    (mt6332_aPMURegWr[u2Cnt][0])  );

       //printk("[Before MASK] %x,%x,%x\r\n",(mt6332_aPMURegWr[u2Cnt][0]), u2PMUReg,(mt6332_aPMURegWr[u2Cnt][1]));       
       //only check value of mask
       u2PMUReg &= mt6332_aPMURegWr_mask[u2Cnt];
       //printk("[After MASK]%x,%x,%x\r\n",(mt6332_aPMURegWr[u2Cnt][0]), u2PMUReg,(mt6332_aPMURegWr[u2Cnt][1]));
       
       default_value_mask = (test_value & mt6332_aPMURegWr_mask[u2Cnt]);

       if(u2PMUReg != default_value_mask)
       {              
           printk("[error] %x,%x(%x),%x,%x,%x,%x\r\n",
            (mt6332_aPMURegWr[u2Cnt][0]),             
            test_value,
            default_value_mask,
            mt6332_aPMURegWr_mask[u2Cnt],
            upmu_get_reg_value(    (mt6332_aPMURegWr[u2Cnt][0])  ),              
            u2PMUReg, 
            default_value_mask
            );
           
           printk("call pwrap_init()\n");
           pwrap_init();
       }
    }

    #if 0 //debug check
    for(u2Cnt = 0; u2Cnt < (sizeof(mt6332_aPMURegWr)/sizeof(*mt6332_aPMURegWr)); ++u2Cnt)
    {
        printk("Reg[%x] %x\n", 
            (mt6332_aPMURegWr[u2Cnt][0]), 
            upmu_get_reg_value(    (mt6332_aPMURegWr[u2Cnt][0])  )
            );
    }
    #endif
}
#endif 

#ifdef EFUSE_DVT_ENABLE
void exec_6331_efuse_test(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int i=0;
    
    printk("[exec_6331_efuse_test] start\n");

    //1. enable efuse ctrl engine clock
    ret=pmic_config_interface(0x0154, 0x0010, 0xFFFF, 0);
    ret=pmic_config_interface(0x0148, 0x0004, 0xFFFF, 0);

    //2.
    ret=pmic_config_interface(0x0616, 0x1, 0x1, 0);

    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        0x0150,upmu_get_reg_value(0x0150),
        0x0144,upmu_get_reg_value(0x0144),
        0x0616,upmu_get_reg_value(0x0616)
        );

    for(i=0;i<=0x1F;i++)
    {
        //3. set row to read
        ret=pmic_config_interface(0x0600, i, 0x1F, 1);

        //4. Toggle
        ret=pmic_read_interface(0x610, &reg_val, 0x1, 0);
        if(reg_val==0)
            ret=pmic_config_interface(0x610, 1, 0x1, 0);
        else
            ret=pmic_config_interface(0x610, 0, 0x1, 0);

        reg_val=1;    
        while(reg_val == 1)
        {
            ret=pmic_read_interface(0x61A, &reg_val, 0x1, 0);
            //printk("5. polling Reg[0x61A][0]=0x%x\n", reg_val);
        }

        //6. read data
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "i=%d,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
            i,
            0x0600,upmu_get_reg_value(0x0600),
            0x061A,upmu_get_reg_value(0x061A),
            0x0618,upmu_get_reg_value(0x0618)
            );
    }

    //7. Disable efuse ctrl engine clock
    ret=pmic_config_interface(0x0146, 0x0004, 0xFFFF, 0);
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x\n",             
        0x0144,upmu_get_reg_value(0x0144)
        );    

    printk("[exec_6331_efuse_test] Done\n");
}

void exec_6332_efuse_test(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int i=0;
    
    printk("[exec_6332_efuse_test] start\n");

    //1. enable efuse ctrl engine clock
    ret=pmic_config_interface(0x80B6, 0x0010, 0xFFFF, 0);
    ret=pmic_config_interface(0x80A4, 0x0004, 0xFFFF, 0);

    //2.
    ret=pmic_config_interface(0x8C6C, 0x1, 0x1, 0);

    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
        0x80B2,upmu_get_reg_value(0x80B2),
        0x80A0,upmu_get_reg_value(0x80A0),
        0x8C6C,upmu_get_reg_value(0x8C6C)
        );

    for(i=0;i<=0x1F;i++)
    {
        //3. set row to read
        ret=pmic_config_interface(0x8C56, i, 0x1F, 1);

        //4. Toggle
        ret=pmic_read_interface(0x8C66, &reg_val, 0x1, 0);
        if(reg_val==0)
            ret=pmic_config_interface(0x8C66, 1, 0x1, 0);
        else
            ret=pmic_config_interface(0x8C66, 0, 0x1, 0);

        reg_val=1;    
        while(reg_val == 1)
        {
            ret=pmic_read_interface(0x8C70, &reg_val, 0x1, 0);
            //printk("5. polling Reg[0x61A][0]=0x%x\n", reg_val);
        }

        //6. read data
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "i=%d,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n", 
            i,
            0x8C56,upmu_get_reg_value(0x8C56),
            0x8C70,upmu_get_reg_value(0x8C70),
            0x8C6E,upmu_get_reg_value(0x8C6E)
            );
    }

    //7. Disable efuse ctrl engine clock
    ret=pmic_config_interface(0x80A2, 0x0004, 0xFFFF, 0);
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x\n",             
        0x80A0,upmu_get_reg_value(0x80A0)
        );

    printk("[exec_6332_efuse_test] Done\n");
}
#endif

//////////////////////////////////////////
// AUXADC
//////////////////////////////////////////
#ifdef AUXADC_DVT_ENABLE
#define MAX_CHANNEL_NUM 22

// trimming
#define GAIN_CH0    0
#define OFFSET_CH0    0
#define GAIN_CH4    0
#define OFFSET_CH4    0
#define GAIN_CH7    0
#define OFFSET_CH7    0
#define SW_GAIN        0
#define SW_OFFSET    0

extern kal_uint32  pmic_is_auxadc_ready(kal_int32 channel_num, upmu_adc_chip_list_enum chip_num, upmu_adc_user_list_enum user_num);
extern kal_uint32  pmic_get_adc_output(kal_int32 channel_num, upmu_adc_chip_list_enum chip_num, upmu_adc_user_list_enum user_num);
extern int PMIC_IMM_GetChannelNumber(upmu_adc_chl_list_enum dwChannel);
extern upmu_adc_chip_list_enum PMIC_IMM_GetChipNumber(upmu_adc_chl_list_enum dwChannel);
extern upmu_adc_user_list_enum PMIC_IMM_GetUserNumber(upmu_adc_chl_list_enum dwChannel);

static upmu_adc_chl_list_enum eChannelEnumList[MAX_CHANNEL_NUM] =
{
    ADC_TSENSE_31_AP,
    ADC_VACCDET_AP,
    ADC_VISMPS_1_AP,
    ADC_ADCVIN0_AP,
    ADC_HP_AP,
    ADC_BATSNS_AP,
    ADC_ISENSE_AP,
    ADC_VBIF_AP,
    ADC_BATON_AP,
    ADC_TSENSE_32_AP,
    ADC_VCHRIN_AP,
    ADC_VISMPS_2_AP,
    ADC_VUSB_AP,
    ADC_M3_REF_AP,
    ADC_SPK_ISENSE_AP,
    ADC_SPK_THR_V_AP,
    ADC_SPK_THR_I_AP,
    ADC_VADAPTOR_AP,
    ADC_TSENSE_31_MD,
    ADC_ADCVIN0_MD,
    ADC_ADCVIN0_GPS,
    ADC_TSENSE_32_MD
};

int auxadc_request_one_channel(int index) {
    int ret = 0;
    switch (index){
    case 0:
        ret = PMIC_IMM_GetOneChannelValue(ADC_TSENSE_31_AP, 1, 0);
        break;
    case 1:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_VACCDET_AP, 1, 0);
        break;
    case 2:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_VISMPS_1_AP, 1, 0);
        break;
    case 3:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_ADCVIN0_AP, 1, 0);
        break;
    case 4:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_HP_AP, 1, 0);
        break;
    case 5:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_BATSNS_AP, 1, 0);
        break;
    case 6:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_ISENSE_AP, 1, 0);
        break;
    case 7:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_VBIF_AP, 1, 0);
        break;
    case 8:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_BATON_AP, 1, 0);
        break;
    case 9:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_TSENSE_32_AP, 1, 0);
        break;
    case 10:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_VCHRIN_AP, 1, 0);
        break;
    case 11:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_VISMPS_2_AP, 1, 0);
        break;
    case 12:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_VUSB_AP, 1, 0);
        break;
    case 13:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_M3_REF_AP, 1, 0);
        break;
    case 14:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_SPK_ISENSE_AP, 1, 0);
        break;
    case 15:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_SPK_THR_V_AP, 1, 0);
        break;
    case 16:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_SPK_THR_I_AP, 1, 0);
        break;
    case 17:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_VADAPTOR_AP, 1, 0);
        break;
    case 18:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_TSENSE_31_MD, 1, 0);
        break;
    case 19:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_ADCVIN0_MD, 1, 0);
        break;
    case 20:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_ADCVIN0_GPS, 1, 0);
        break;
    case 21:    
        ret = PMIC_IMM_GetOneChannelValue(ADC_TSENSE_32_MD, 1, 0);
        break;
    default :
        printk("[auxadc_request_test] no such channel number!!\n");
        break;
    }    
    
    return ret;    
}

void auxadc_request_test(void)
{
    int i;
    int ret[MAX_CHANNEL_NUM] = {0};
    printk("[auxadc_request_test] start \n");
    
    for (i= 0; i < MAX_CHANNEL_NUM; i++) {
        
        
    }
    printk("[auxadc_request_test] Process Result: ");
    for (i=0; i< 21; i++) {
        ret[i] = auxadc_request_one_channel(i);
        printk("ret[%d]: %d ", i, ret[i]);
    }
    printk("\n[auxadc_request_test] end !! \n");
}

void auxadc_trimm_channel_test(void)
{
    int i, j;
    int ret[MAX_CHANNEL_NUM] = {0}, ret_raw[MAX_CHANNEL_NUM] = {0};
    printk("[auxadc_trimm_channel_test] start\n");
    
    mt6331_upmu_set_efuse_gain_ch4_trim(GAIN_CH4);
    mt6331_upmu_set_efuse_offset_ch4_trim(OFFSET_CH4);
    mt6331_upmu_set_efuse_gain_ch7_trim(GAIN_CH7);
    mt6331_upmu_set_efuse_offset_ch7_trim(OFFSET_CH7);
    mt6332_upmu_set_efuse_gain_ch4_trim(GAIN_CH4);
    mt6332_upmu_set_efuse_offset_ch4_trim(OFFSET_CH4);
    mt6332_upmu_set_efuse_gain_ch0_trim(GAIN_CH0);
    mt6332_upmu_set_efuse_offset_ch0_trim(OFFSET_CH0);
    mt6331_upmu_set_auxadc_sw_gain_trim(SW_GAIN);
    mt6331_upmu_set_auxadc_sw_offset_trim(SW_OFFSET);
    mt6332_upmu_set_auxadc_sw_gain_trim(SW_GAIN);
    mt6332_upmu_set_auxadc_sw_offset_trim(SW_OFFSET);

    for (i= 0; i < MAX_CHANNEL_NUM; i++) {
        for (j = 0; j < 4; j++) {
            mt6331_upmu_set_auxadc_trim_ch0_sel(j);
            ret[i] = auxadc_request_one_channel(i);
            if (i < 8) 
                ret_raw[i] = mt6331_upmu_get_auxadc_adc_out_raw();
            else if (i < MAX_CHANNEL_NUM)
                ret_raw[i] = mt6332_upmu_get_auxadc_adc_out_raw();
            printk("ret[%d]: %d , ret_raw[%d]: %d ", i, ret[i], i, ret_raw[i]);    
        }    
    }
}
kal_int32 dvtloop=50;

void do_auxadc_polling(int number, int chip_info, int user_info, int *channel_num, upmu_adc_chip_list_enum *chip_num, upmu_adc_user_list_enum *user_num)
{

    kal_int32 i=0, j =0;
    kal_int32 ret_data[MAX_CHANNEL_NUM];
    kal_int32 ret_order[MAX_CHANNEL_NUM];
    kal_int16 order=1;
    
    printk("[do_auxadc_polling] start\n");
    for(i=0;i<MAX_CHANNEL_NUM;i++)
    {
        ret_data[i]=0;
        ret_order[i]=0;
    }
    
    /*
        MT6331
        0 : NA
        1 : NA
        2 : NA 
        3 : NA
        4 : TSENSE_PMIC
        5 : VACCDET
        6 : VISMPS_1
        7 : AUXADCVIN0
        8 : NA    
        9 : HP
        11-15: Shared
        
        MT6332
        0 : BATSNS
        1 : ISENSE
        2 : VBIF 
        3 : BATON
        4 : TSENSE_BAT
        5 : VCHRIN
        6 : VISMPS_2
        7 : VUSB/ VADAPTOR
        8 : M3_REF    
        9 : SPK_ISENSE
        10: SPK_THR_V
        11: SPK_THR_I
        12-15: shared 
    */   
    do {
        for (i=0; i < MAX_CHANNEL_NUM; i++) {
            if (chip_num[i] != chip_info || (((0x1 << user_num[i]) & (user_info)) == 0))
                continue;
            if (ret_order[i]==0 && pmic_is_auxadc_ready(channel_num[i], chip_num[i], user_num[i]) == 1 )
            {
                ret_data[i] = pmic_get_adc_output(channel_num[i], chip_num[i], user_num[i]);    
                ret_order[i]=order;
                order++;
            }
        }
        if (i>dvtloop)
            break;
        for (j=0; j < MAX_CHANNEL_NUM; j++) {
            printk("[%d] ", pmic_is_auxadc_ready(channel_num[i], chip_num[i], user_num[i]));        
        }
        printk("\n");
    } while (order<=number);
#if 0
    printk("ch0:BATON2    order[%2d] val[0x%4x] [%4d]\n ", ret_order[0], ret_data[0],ret_data[0]*VOLTAGE_FULL_RANGE/ADC_PRECISE);   
    printk("ch1:CH6        order[%2d] val[0x%4x] [%4d]\n ", ret_order[1], ret_data[1],ret_data[1]*VOLTAGE_FULL_RANGE/ADC_PRECISE); 
    printk("ch2:THR2        order[%2d] val[0x%4x] [%4d]\n ", ret_order[2], ret_data[2],ret_data[2]*VOLTAGE_FULL_RANGE/ADC_PRECISE); 
    printk("ch3:THR1        order[%2d] val[0x%4x] [%4d]\n ", ret_order[3], ret_data[3],ret_data[3]*VOLTAGE_FULL_RANGE/ADC_PRECISE); 
    printk("ch4:VCDT        order[%2d] val[0x%4x] [%4d]\n ", ret_order[4], ret_data[4],ret_data[4]*VOLTAGE_FULL_RANGE/ADC_PRECISE); 
    printk("ch5:BATON1    order[%2d] val[0x%4x] [%4d]\n ", ret_order[5], ret_data[5],ret_data[5]*4*VOLTAGE_FULL_RANGE/ADC_PRECISE); 
    printk("ch6:ISENSE    order[%2d] val[0x%4x] [%4d]\n ", ret_order[6], ret_data[6],ret_data[6]*4*VOLTAGE_FULL_RANGE/ADC_PRECISE); 
    printk("ch7:BATSNS    order[%2d] val[0x%4x] [%4d]\n ", ret_order[7], ret_data[7],ret_data[7]*4*VOLTAGE_FULL_RANGE/ADC_PRECISE); 
    printk("ch8:ACCDET    order[%2d] val[0x%4x] [%4d]\n ", ret_order[8], ret_data[8],ret_data[8]*VOLTAGE_FULL_RANGE/ADC_PRECISE); 
    printk("gps:            order[%2d] val[0x%4x] [%4d]\n ", ret_order[9], ret_data[9],ret_data[9]*VOLTAGE_FULL_RANGE/ADC_PRECISE/2); 
       printk("md:            order[%2d] val[0x%4x] [%4d]\n ", ret_order[10], ret_data[10],ret_data[10]*VOLTAGE_FULL_RANGE/ADC_PRECISE/2); 

    printk("[%d][%d][%d][%d][%d][%d][%d][%d][%d][%d][%d]\n ", 
        upmu_get_rg_adc_rdy_baton2(),
        upmu_get_rg_adc_rdy_ch6(),
        upmu_get_rg_adc_rdy_thr_sense1(),
        upmu_get_rg_adc_rdy_thr_sense2(),
        upmu_get_rg_adc_rdy_vcdt(),
        upmu_get_rg_adc_rdy_baton1(),
        upmu_get_rg_adc_rdy_isense(),
        upmu_get_rg_adc_rdy_batsns(),
        upmu_get_rg_adc_rdy_ch5(),
        upmu_get_rg_adc_rdy_gps(),
        upmu_get_rg_adc_rdy_md());
#endif
}

void auxadc_set_avg_num_max(void)
{
    //set SMPS CLK from external CLK 
    //MT6332 External CLK input : PAD_PWM_IN 
    //MT6331 External CLK input : PAD_HOMEKEY 

    mt6331_upmu_set_rg_smps_ck_tstsel(1);    
    //Set AVG as largest setting
    mt6331_upmu_set_auxadc_avg_num_sel(0x0FFF);
    mt6331_upmu_set_auxadc_avg_num_sel_lbat(0x1);
    mt6331_upmu_set_auxadc_avg_num_sel_wakeup(0x1);
}

void auxadc_priority_test_6331(void)
{
    int i, ret = 0;     
        int channel_num[MAX_CHANNEL_NUM];
        upmu_adc_chip_list_enum chip_num[MAX_CHANNEL_NUM];
        upmu_adc_user_list_enum user_num[MAX_CHANNEL_NUM];
        
        auxadc_set_avg_num_max();
        for (i=0; i < MAX_CHANNEL_NUM; i++) {
            channel_num[i] = PMIC_IMM_GetChannelNumber(eChannelEnumList[i]);
        chip_num[i] = PMIC_IMM_GetChipNumber(eChannelEnumList[i]);
        user_num[i] = PMIC_IMM_GetUserNumber(eChannelEnumList[i]);
        }      
        
        ret=pmic_config_interface( (kal_uint32)(MT6331_AUXADC_RQST0_SET), 0x0FFF, 0x0FFF, 0); 
        ret=pmic_config_interface( (kal_uint32)(MT6331_AUXADC_RQST1_SET), 0x0190, 0x0190, 0);
        
    do_auxadc_polling(8, MT6331_CHIP, 0x111, channel_num, chip_num, user_num);               
}

void auxadc_priority_test_6332(void)
{
    int i, ret = 0;     
        int channel_num[MAX_CHANNEL_NUM];
        upmu_adc_chip_list_enum chip_num[MAX_CHANNEL_NUM];
        upmu_adc_user_list_enum user_num[MAX_CHANNEL_NUM];
        
        auxadc_set_avg_num_max();
        for (i=0; i < MAX_CHANNEL_NUM; i++) {
            channel_num[i] = PMIC_IMM_GetChannelNumber(eChannelEnumList[i]);
        chip_num[i] = PMIC_IMM_GetChipNumber(eChannelEnumList[i]);
        user_num[i] = PMIC_IMM_GetUserNumber(eChannelEnumList[i]);
        }      
        
        ret=pmic_config_interface( (kal_uint32)(MT6332_AUXADC_RQST0_SET), 0x0FFF, 0x0FFF, 0); 
        ret=pmic_config_interface( (kal_uint32)(MT6332_AUXADC_RQST1_SET), 0x0190, 0x0190, 0);
        
    do_auxadc_polling(12, MT6332_CHIP, 0x111,channel_num, chip_num, user_num);               
}

void auxadc_priority_test_GPSMD1_6331(void)
{
    int i, ret = 0;     
        int channel_num[MAX_CHANNEL_NUM];
        upmu_adc_chip_list_enum chip_num[MAX_CHANNEL_NUM];
        upmu_adc_user_list_enum user_num[MAX_CHANNEL_NUM];
        
        auxadc_set_avg_num_max();
        for (i=0; i < MAX_CHANNEL_NUM; i++) {
            channel_num[i] = PMIC_IMM_GetChannelNumber(eChannelEnumList[i]);
        chip_num[i] = PMIC_IMM_GetChipNumber(eChannelEnumList[i]);
        user_num[i] = PMIC_IMM_GetUserNumber(eChannelEnumList[i]);
        }      
        
        ret=pmic_config_interface( (kal_uint32)(MT6331_AUXADC_RQST0_SET), 0x007F, 0x007F, 0); 
        udelay(1);
        ret=pmic_config_interface( (kal_uint32)(MT6331_AUXADC_RQST1_SET), 0x0190, 0x0190, 0);
        
    do_auxadc_polling(8, MT6331_CHIP, 0x111, channel_num, chip_num, user_num);               
}

void auxadc_priority_test_GPSMD2_6331(void)
{
    int i, ret = 0;     
        int channel_num[MAX_CHANNEL_NUM];
        upmu_adc_chip_list_enum chip_num[MAX_CHANNEL_NUM];
        upmu_adc_user_list_enum user_num[MAX_CHANNEL_NUM];
        
        auxadc_set_avg_num_max();
        for (i=0; i < MAX_CHANNEL_NUM; i++) {
            channel_num[i] = PMIC_IMM_GetChannelNumber(eChannelEnumList[i]);
        chip_num[i] = PMIC_IMM_GetChipNumber(eChannelEnumList[i]);
        user_num[i] = PMIC_IMM_GetUserNumber(eChannelEnumList[i]);
        }      
        
        ret=pmic_config_interface( (kal_uint32)(MT6331_AUXADC_RQST0_SET), 0x0100, 0x0100, 0); 
        udelay(1);
        ret=pmic_config_interface( (kal_uint32)(MT6331_AUXADC_RQST1_SET), 0x0080, 0x0080, 0);
        
    do_auxadc_polling(8, MT6331_CHIP, 0x110, channel_num, chip_num, user_num);               
}

void auxadc_priority_test_GPSMD3_6331(void)
{
    int i, ret = 0;     
        int channel_num[MAX_CHANNEL_NUM];
        upmu_adc_chip_list_enum chip_num[MAX_CHANNEL_NUM];
        upmu_adc_user_list_enum user_num[MAX_CHANNEL_NUM];
        
        auxadc_set_avg_num_max();
        for (i=0; i < MAX_CHANNEL_NUM; i++) {
            channel_num[i] = PMIC_IMM_GetChannelNumber(eChannelEnumList[i]);
        chip_num[i] = PMIC_IMM_GetChipNumber(eChannelEnumList[i]);
        user_num[i] = PMIC_IMM_GetUserNumber(eChannelEnumList[i]);
        }      
        
        ret=pmic_config_interface( (kal_uint32)(MT6331_AUXADC_RQST0_SET), 0x0100, 0x0100, 0); 
        if (pmic_is_auxadc_ready(8, 0, 2)) {
            ret=pmic_config_interface( (kal_uint32)(MT6331_AUXADC_RQST1_SET), 0x0080, 0x0080, 0);
            if (pmic_is_auxadc_ready(7, 0, 1)) {
                pmic_get_adc_output(7, 0, 1);
            }
        }              
}

#define LBAT_TEST 0
#define HBAT_TEST 1

#define WAKEUP_MODE 0
#define SLEEP_MODE 1


kal_uint32 LBAT_VOLT_MAX=0x0A80; // wait SA provide, 4.2V
kal_uint32 LBAT_VOLT_MIN=0x0880; // wait SA provide, 3.4V

kal_uint32 LBAT_DET_PRD_19_16=0x0; 
kal_uint32 LBAT_DET_PRD_15_0=0x2710; // 1s


kal_uint32 LBAT_DEBT_MAX=1;
kal_uint32 LBAT_DEBT_MIN=1;

extern kal_uint32 mt6331_bat_l_int_handler(void);
extern kal_uint32 mt6331_bat_h_int_handler(void);
extern kal_uint32 mt6332_bat_l_int_handler(void);
extern kal_uint32 mt6332_bat_h_int_handler(void);

void auxadc_battery_int_protection_6331(U32 test_type, U32 test_mode)
{
    
    U32 lbat_debounce_count_max=0;
    U32 lbat_debounce_count_min=0;
    U32 timeout_count = 0;
    U32 adc_out_lbat = 0;

    //init
    mt6331_upmu_set_auxadc_lbat_irq_en_max(0);
    mt6331_upmu_set_auxadc_lbat_irq_en_min(0);    
    mt6331_upmu_set_auxadc_lbat_en_max(0);
    mt6331_upmu_set_auxadc_lbat_en_min(0);
      mt6331_upmu_set_rg_int_en_bat_l(0);
    mt6331_upmu_set_rg_int_en_bat_h(0);
    
    //0. set issue interrupt
    mt6331_upmu_set_rg_int_en_bat_l(1);
    mt6331_upmu_set_rg_int_en_bat_h(1);
    //1. setup max voltage treshold as VBAT = 4.2
    mt6331_upmu_set_auxadc_lbat_volt_max(LBAT_VOLT_MAX);
    //2. setup min voltage treshold as VBAT = 3.4
    mt6331_upmu_set_auxadc_lbat_volt_min(LBAT_VOLT_MIN);
    //3. setup detection period
    mt6331_upmu_set_auxadc_lbat_det_prd_19_16(LBAT_DET_PRD_19_16);
    mt6331_upmu_set_auxadc_lbat_det_prd_15_0(LBAT_DET_PRD_15_0);
    //4. setup max./min. debounce time.
    mt6331_upmu_set_auxadc_lbat_debt_max(LBAT_DEBT_MAX);
    mt6331_upmu_set_auxadc_lbat_debt_min(LBAT_DEBT_MIN);
    
    
    if (test_type == LBAT_TEST) {
        //5. turn on IRQ
        mt6331_upmu_set_auxadc_lbat_irq_en_max(0);
        mt6331_upmu_set_auxadc_lbat_irq_en_min(1);
    
        //6. turn on LowBattery Detection
        mt6331_upmu_set_auxadc_lbat_en_max(0);
        mt6331_upmu_set_auxadc_lbat_en_min(1);
    }else if (test_type == HBAT_TEST) {
        //5. turn on IRQ
        mt6331_upmu_set_auxadc_lbat_irq_en_max(1);
        mt6331_upmu_set_auxadc_lbat_irq_en_min(0);
    
        //6. turn on LowBattery Detection
        mt6331_upmu_set_auxadc_lbat_en_max(1);
        mt6331_upmu_set_auxadc_lbat_en_min(0);
    }
    //7. Monitor Debounce counts
    lbat_debounce_count_max = mt6331_upmu_get_auxadc_lbat_debounce_count_max();
    lbat_debounce_count_min = mt6331_upmu_get_auxadc_lbat_debounce_count_min();
    
    while (!mt6331_upmu_get_auxadc_adc_rdy_lbat()) {
        mdelay(1000);
        timeout_count++;
        printk("[auxadc_battery_int_protection_6331] wait for lbat ready! timeout_count = (%d)\n", timeout_count);
        if (timeout_count > 10000) break;    
    } 
    adc_out_lbat = mt6331_upmu_get_auxadc_adc_out_lbat();
    
    //9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt  -- LBAT_TEST
    //9. Test on VBAT = 4.0 -> 4.2 -> 4.3 and receive interrupt  -- HBAT_TEST
    printk("[auxadc_battery_int_protection_6331] setting .. done (adc_out_lbat=%d, lbat_debounce_count_max=%d, lbat_debounce_count_min=%d) \n", 
        adc_out_lbat, lbat_debounce_count_max, lbat_debounce_count_min);
        

    if (test_mode == SLEEP_MODE) {
        printk("[auxadc_battery_int_protection_6331] sleep mode\n");
        //0.0 wakeup start setting for sleep mode
        mt6331_upmu_set_strup_auxadc_start_sel(0);
        mt6331_upmu_set_strup_auxadc_rstb_sw(1);
        mt6331_upmu_set_strup_auxadc_rstb_sel(1);
        //0.1 vref18 turned off setting for sleep mode
        //0:sleep mode
            //1:normal mode
        set_srclken_sw_mode();
        set_srclken_1_val(0);
    }
}

void mt6331_bat_int_close(void)
{
    mt6331_upmu_set_auxadc_lbat_irq_en_max(0);
    mt6331_upmu_set_auxadc_lbat_irq_en_min(0);
    mt6331_upmu_set_auxadc_lbat_en_max(0);
    mt6331_upmu_set_auxadc_lbat_en_min(0);
    mt6331_upmu_set_rg_int_en_bat_l(0);
    mt6331_upmu_set_rg_int_en_bat_h(0);
            
    printk("[mt6331_bat_int_close] done\n");
}

void auxadc_battery_int_protection_6332(U32 test_type, U32 test_mode)
{
    
    U32 lbat_debounce_count_max=0;
    U32 lbat_debounce_count_min=0;
    U32 timeout_count = 0;
    U32 adc_out_lbat = 0;
    
    if (test_mode == SLEEP_MODE) {
        printk("[auxadc_battery_int_protection_6332] sleep mode\n");
        //0.0 wakeup start setting for sleep mode
        mt6332_upmu_set_strup_auxadc_start_sel(0);
        mt6332_upmu_set_strup_auxadc_rstb_sw(1);
        mt6332_upmu_set_strup_auxadc_rstb_sel(1);
        mt6332_upmu_set_strup_con8_rsv0(1);
    }
    printk("[auxadc_battery_int_protection_6332] E1 workaround\n");
    mt6332_upmu_set_rg_adcin_batsns_en(1);
    mt6332_upmu_set_rg_adcin_cs_en(1);
    
    if (test_mode == SLEEP_MODE) {
        printk("[auxadc_battery_int_protection_6332] sleep mode\n");
        //0.0 wakeup start setting for sleep mode
        mt6332_upmu_set_strup_auxadc_start_sel(0);
        mt6332_upmu_set_strup_auxadc_rstb_sw(1);
        mt6332_upmu_set_strup_auxadc_rstb_sel(1);
        mt6332_upmu_set_strup_con8_rsv0(1);        
    }
    //init
    mt6332_upmu_set_auxadc_lbat_irq_en_max(0);
    mt6332_upmu_set_auxadc_lbat_irq_en_min(0);    
    mt6332_upmu_set_auxadc_lbat_en_max(0);
    mt6332_upmu_set_auxadc_lbat_en_min(0);
      mt6332_upmu_set_rg_int_en_bat_l(0);
    mt6332_upmu_set_rg_int_en_bat_h(0);
    
    //0. set issue interrupt
    mt6332_upmu_set_rg_int_en_bat_l(1);
    mt6332_upmu_set_rg_int_en_bat_h(1);
    //1. setup max voltage treshold as VBAT = 4.2
    mt6332_upmu_set_auxadc_lbat_volt_max(LBAT_VOLT_MAX);
    //2. setup min voltage treshold as VBAT = 3.4
    mt6332_upmu_set_auxadc_lbat_volt_min(LBAT_VOLT_MIN);
    //3. setup detection period
    mt6332_upmu_set_auxadc_lbat_det_prd_19_16(LBAT_DET_PRD_19_16);
    mt6332_upmu_set_auxadc_lbat_det_prd_15_0(LBAT_DET_PRD_15_0);
    //4. setup max./min. debounce time.
    mt6332_upmu_set_auxadc_lbat_debt_max(LBAT_DEBT_MAX);
    mt6332_upmu_set_auxadc_lbat_debt_min(LBAT_DEBT_MIN);
    
    if (test_type == LBAT_TEST) {
        //5. turn on IRQ
        mt6332_upmu_set_auxadc_lbat_irq_en_max(0);
        mt6332_upmu_set_auxadc_lbat_irq_en_min(1);
    
        //6. turn on LowBattery Detection
        mt6332_upmu_set_auxadc_lbat_en_max(0);
        mt6332_upmu_set_auxadc_lbat_en_min(1);
    }else if (test_type == HBAT_TEST) {
        //5. turn on IRQ
        mt6332_upmu_set_auxadc_lbat_irq_en_max(1);
        mt6332_upmu_set_auxadc_lbat_irq_en_min(0);
    
        //6. turn on LowBattery Detection
        mt6332_upmu_set_auxadc_lbat_en_max(1);
        mt6332_upmu_set_auxadc_lbat_en_min(0);
    }
    //7. Monitor Debounce counts
    lbat_debounce_count_max = mt6332_upmu_get_auxadc_lbat_debounce_count_max();
    lbat_debounce_count_min = mt6332_upmu_get_auxadc_lbat_debounce_count_min();
    
    while (!mt6332_upmu_get_auxadc_adc_rdy_lbat()) {
        mdelay(1000);
        timeout_count++;
        printk("[auxadc_battery_int_protection_6332] wait for lbat ready! timeout_count = (%d)\n", timeout_count);
        if (timeout_count > 10000) break;    
    } 
    adc_out_lbat = mt6332_upmu_get_auxadc_adc_out_lbat();
    
    //9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt  -- LBAT_TEST
    //9. Test on VBAT = 4.0 -> 4.2 -> 4.3 and receive interrupt  -- HBAT_TEST
    printk("[auxadc_battery_int_protection_6332] setting .. done (adc_out_lbat=%d, lbat_debounce_count_max=%d, lbat_debounce_count_min=%d) \n", 
        adc_out_lbat, lbat_debounce_count_max, lbat_debounce_count_min);
    if (test_mode == SLEEP_MODE) {
        printk("[auxadc_battery_int_protection_6332] sleep mode\n");
        //0.0 wakeup start setting for sleep mode
        mt6332_upmu_set_strup_auxadc_start_sel(0);
        mt6332_upmu_set_strup_auxadc_rstb_sw(1);
        mt6332_upmu_set_strup_auxadc_rstb_sel(1);
        mt6332_upmu_set_strup_con8_rsv0(1);
        //0:sleep mode
            //1:normal mode
        set_srclken_sw_mode();
        set_srclken_1_val(0);
    }
}

void mt6332_bat_int_close(void)
{
        mt6332_upmu_set_auxadc_lbat_irq_en_max(0);
        mt6332_upmu_set_auxadc_lbat_irq_en_min(0);
        mt6332_upmu_set_auxadc_lbat_en_max(0);
        mt6332_upmu_set_auxadc_lbat_en_min(0);
        mt6332_upmu_set_rg_int_en_bat_l(0);
        mt6332_upmu_set_rg_int_en_bat_h(0);
            
    printk("[mt6332_bat_int_close] done\n");
}

//2.1.1 test low battery voltage interrupt
void auxadc_low_battery_int_test_6331(void) 
{
    auxadc_battery_int_protection_6331(LBAT_TEST, WAKEUP_MODE);
}
//2.1.2 : Test high battery voltage interrupt [DVT]
void auxadc_high_battery_int_test_6331(void) 
{
    auxadc_battery_int_protection_6331(HBAT_TEST, WAKEUP_MODE);
}
//2.1.3 : test low battery voltage interrupt  in sleep mode [DVT]
void auxadc_low_battery_int2_test_6331(void) 
{
    auxadc_battery_int_protection_6331(LBAT_TEST, SLEEP_MODE);
}
//2.1.4 : test hig battery voltage interrupt  in sleep mode [DVT]
void auxadc_high_battery_int2_test_6331(void) 
{
    auxadc_battery_int_protection_6331(HBAT_TEST, SLEEP_MODE);
}
//2.1.1 test low battery voltage interrupt
void auxadc_low_battery_int_test_6332(void) 
{
    auxadc_battery_int_protection_6332(LBAT_TEST, WAKEUP_MODE);
}
//2.1.2 : Test high battery voltage interrupt [DVT]
void auxadc_high_battery_int_test_6332(void) 
{
    auxadc_battery_int_protection_6332(HBAT_TEST, WAKEUP_MODE);
}
//2.1.3 : test low battery voltage interrupt  in sleep mode [DVT]
void auxadc_low_battery_int2_test_6332(void) 
{
    auxadc_battery_int_protection_6332(LBAT_TEST, SLEEP_MODE);
}
//2.1.4 : test hig battery voltage interrupt  in sleep mode [DVT]
void auxadc_high_battery_int2_test_6332(void) 
{
    auxadc_battery_int_protection_6332(HBAT_TEST, SLEEP_MODE);
}


//25700mv -20~80װ϶, ftū׫Y O -1.7mV / degree
#define LTHR_TEST 0
#define HTHR_TEST 1

kal_uint32 THR_VOLT_MAX=0x0340; // wait SA provide, 50
kal_uint32 THR_VOLT_MIN=0x03c0; // wait SA provide, -10

kal_uint32 THR_DET_PRD_19_16=0x0; 
kal_uint32 THR_DET_PRD_15_0=0x2710; // 10s


kal_uint32 THR_DEBT_MAX=1;
kal_uint32 THR_DEBT_MIN=1;

extern kal_uint32 mt6331_thr_l_int_handler(void);
extern kal_uint32 mt6331_thr_h_int_handler(void);
extern kal_uint32 mt6332_thr_l_int_handler(void);
extern kal_uint32 mt6332_thr_h_int_handler(void);
void auxadc_thermal_int_protection_6331(U32 test_type, U32 test_mode)
{
    
    U32 thr_debounce_count_max=0;
    U32 thr_debounce_count_min=0;
    U32 timeout_count = 0;
    U32 adc_out_thr = 0;


    //0. set issue interrupt
    mt6331_upmu_set_rg_int_en_thr_l(1);
    mt6331_upmu_set_rg_int_en_thr_h(1);
    //1. setup max voltage treshold as VBAT = 4.2
    mt6331_upmu_set_auxadc_thr_volt_max(THR_VOLT_MAX);
    //2. setup min voltage treshold as VBAT = 3.4
    mt6331_upmu_set_auxadc_thr_volt_min(THR_VOLT_MIN);
    //3. setup detection period
    mt6331_upmu_set_auxadc_thr_det_prd_19_16(THR_DET_PRD_19_16);
    mt6331_upmu_set_auxadc_thr_det_prd_15_0(THR_DET_PRD_15_0);
    //4. setup max./min. debounce time.
    mt6331_upmu_set_auxadc_thr_debt_max(THR_DEBT_MAX);
    mt6331_upmu_set_auxadc_thr_debt_min(THR_DEBT_MIN);
    
    if (test_type == LTHR_TEST) {
        //5. turn on IRQ
        mt6331_upmu_set_auxadc_thr_irq_en_max(0);
        mt6331_upmu_set_auxadc_thr_irq_en_min(1);
    
        //6. turn on LowThermal Detection
        mt6331_upmu_set_auxadc_thr_en_max(0);
        mt6331_upmu_set_auxadc_thr_en_min(1);
    }else if (test_type == HTHR_TEST) {
        //5. turn on IRQ
        mt6331_upmu_set_auxadc_thr_irq_en_max(1);
        mt6331_upmu_set_auxadc_thr_irq_en_min(0);
    
        //6. turn on HighThermal Detection
        mt6331_upmu_set_auxadc_thr_en_max(1);
        mt6331_upmu_set_auxadc_thr_en_min(0);
    }
    //7. Monitor Debounce counts
    thr_debounce_count_max = mt6331_upmu_get_auxadc_thr_debounce_count_max();
    thr_debounce_count_min = mt6331_upmu_get_auxadc_thr_debounce_count_min();
    
    while (!mt6331_upmu_get_auxadc_adc_rdy_thr_hw()) {
        timeout_count++;
        printk("[auxadc_thermal_int_protection_6331] wait for lbat ready! timeout_count = (%d)\n", timeout_count);
        if (timeout_count > 10000) break;    
    } 
    adc_out_thr = mt6331_upmu_get_auxadc_adc_out_thr_hw();
    
    //9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt  -- LBAT_TEST
    //9. Test on VBAT = 4.0 -> 4.2 -> 4.3 and receive interrupt  -- HBAT_TEST
    printk("[auxadc_thermal_int_protection_6331] done (adc_out_thr=%d, thr_debounce_count_max=%d, thr_debounce_count_min=%d) \n", 
        adc_out_thr, thr_debounce_count_max, thr_debounce_count_min);
        
        if (test_mode == SLEEP_MODE) {
        printk("[auxadc_thermal_int_protection_6331] sleep mode\n");
        //0.0 wakeup start setting for sleep mode
        mt6331_upmu_set_strup_auxadc_start_sel(0);
        mt6331_upmu_set_strup_auxadc_rstb_sw(1);
        mt6331_upmu_set_strup_auxadc_rstb_sel(1);
        //0.1 vref18 turned off setting for sleep mode    
        
        //0:sleep mode
        //1:normal mode
        set_srclken_sw_mode();
        set_srclken_1_val(0);    
    }
}

void mt6331_thr_int_close(void){
            mt6331_upmu_set_auxadc_thr_irq_en_max(0);
            mt6331_upmu_set_auxadc_thr_irq_en_min(0);
            mt6331_upmu_set_auxadc_thr_en_max(0);
            mt6331_upmu_set_auxadc_thr_en_min(0);
            mt6331_upmu_set_rg_int_en_thr_l(0);
            mt6331_upmu_set_rg_int_en_thr_h(0);
            
            printk("[mt6331_thr_int_close] done\n");
}

void auxadc_thermal_int_protection_6332(U32 test_type, U32 test_mode)
{
    
    U32 thr_debounce_count_max=0;
    U32 thr_debounce_count_min=0;
    U32 timeout_count = 0;
    U32 adc_out_thr = 0;

    //0. set issue interrupt
    mt6332_upmu_set_rg_int_en_thr_l(1);
    mt6332_upmu_set_rg_int_en_thr_h(1);
    //1. setup max voltage treshold as VBAT = 4.2
    mt6332_upmu_set_auxadc_thr_volt_max(THR_VOLT_MAX);
    //2. setup min voltage treshold as VBAT = 3.4
    mt6332_upmu_set_auxadc_thr_volt_min(THR_VOLT_MIN);
    //3. setup detection period
    mt6332_upmu_set_auxadc_thr_det_prd_19_16(THR_DET_PRD_19_16);
    mt6332_upmu_set_auxadc_thr_det_prd_15_0(THR_DET_PRD_15_0);
    //4. setup max./min. debounce time.
    mt6332_upmu_set_auxadc_thr_debt_max(THR_DEBT_MAX);
    mt6332_upmu_set_auxadc_thr_debt_min(THR_DEBT_MIN);
    
    if (test_type == LTHR_TEST) {
        //5. turn on IRQ
        mt6332_upmu_set_auxadc_thr_irq_en_max(0);
        mt6332_upmu_set_auxadc_thr_irq_en_min(1);
    
        //6. turn on LowThermal Detection
        mt6332_upmu_set_auxadc_thr_en_max(0);
        mt6332_upmu_set_auxadc_thr_en_min(1);
    }else if (test_type == HTHR_TEST) {
        //5. turn on IRQ
        mt6332_upmu_set_auxadc_thr_irq_en_max(1);
        mt6332_upmu_set_auxadc_thr_irq_en_min(0);
    
        //6. turn on HighThermal Detection
        mt6332_upmu_set_auxadc_thr_en_max(1);
        mt6332_upmu_set_auxadc_thr_en_min(0);
    }
    //7. Monitor Debounce counts
    thr_debounce_count_max = mt6332_upmu_get_auxadc_thr_debounce_count_max();
    thr_debounce_count_min = mt6332_upmu_get_auxadc_thr_debounce_count_min();
    
    while (!mt6332_upmu_get_auxadc_adc_rdy_thr_hw()) {
        timeout_count++;
        printk("[auxadc_thermal_int_protection_6332] wait for lbat ready! timeout_count = (%d)\n", timeout_count);
        if (timeout_count > 10000) break;    
    } 
    adc_out_thr = mt6332_upmu_get_auxadc_adc_out_thr_hw();
    
    //9. Test on VBAT = 3.5 -> 3.4 -> 3.3 and receive interrupt  -- LTHR_TEST
    //9. Test on VBAT = 4.0 -> 4.2 -> 4.3 and receive interrupt  -- HTHR_TEST
    printk("[auxadc_thermal_int_protection_6332] done (adc_out_thr=%d, thr_debounce_count_max=%d, thr_debounce_count_min=%d) \n", 
        adc_out_thr, thr_debounce_count_max, thr_debounce_count_min);
        
    if (test_mode == SLEEP_MODE) {
        printk("[auxadc_thermal_int_protection_6332] sleep mode\n");
        //0.0 wakeup start setting for sleep mode
        mt6332_upmu_set_strup_auxadc_start_sel(0);
        mt6332_upmu_set_strup_auxadc_rstb_sw(1);
        mt6332_upmu_set_strup_auxadc_rstb_sel(1);
        mt6332_upmu_set_strup_con8_rsv0(1);
        //0:sleep mode
            //1:normal mode
        set_srclken_sw_mode();
        set_srclken_1_val(0);
        //0.1 vref18 turned off setting for sleep mode        
    }
}

void mt6332_thr_int_close(void)
{
            mt6332_upmu_set_auxadc_thr_irq_en_max(0);
            mt6332_upmu_set_auxadc_thr_irq_en_min(0);
            mt6332_upmu_set_auxadc_thr_en_max(0);
            mt6332_upmu_set_auxadc_thr_en_min(0);
            mt6332_upmu_set_rg_int_en_thr_l(0);
            mt6332_upmu_set_rg_int_en_thr_h(0);
            
            printk("[mt6332_thr_int_close] done\n");
}

//2.1.1 test low thermal voltage interrupt
void auxadc_low_thermal_int_test_6331(void) 
{
    auxadc_thermal_int_protection_6331(LTHR_TEST, WAKEUP_MODE);
}
//2.1.2 : Test high thermal voltage interrupt [DVT]
void auxadc_high_thermal_int_test_6331(void) 
{
    auxadc_thermal_int_protection_6331(HTHR_TEST, WAKEUP_MODE);
}
//2.1.3 : test low thermal voltage interrupt  in sleep mode [DVT]
void auxadc_low_thermal_int2_test_6331(void) 
{
    auxadc_thermal_int_protection_6331(LTHR_TEST, SLEEP_MODE);
}
//2.1.4 : test hig thermal voltage interrupt  in sleep mode [DVT]
void auxadc_high_thermal_int2_test_6331(void) 
{
    auxadc_thermal_int_protection_6331(HTHR_TEST, SLEEP_MODE);
}
//2.1.1 test low thermal voltage interrupt
void auxadc_low_thermal_int_test_6332(void) 
{
    auxadc_thermal_int_protection_6332(LTHR_TEST, WAKEUP_MODE);
}
//2.1.2 : Test high thermal voltage interrupt [DVT]
void auxadc_high_thermal_int_test_6332(void) 
{
    auxadc_thermal_int_protection_6332(HTHR_TEST, WAKEUP_MODE);
}
//2.1.3 : test low thermal voltage interrupt  in sleep mode [DVT]
void auxadc_low_thermal_int2_test_6332(void) 
{
    auxadc_thermal_int_protection_6332(LTHR_TEST, SLEEP_MODE);
}
//2.1.4 : test hig thermal voltage interrupt  in sleep mode [DVT]
void auxadc_high_thermal_int2_test_6332(void) 
{
    auxadc_thermal_int_protection_6332(HTHR_TEST, SLEEP_MODE);
}


//WAKEUP1.1.1 power on measurement (HW ZCV) [DVG/HQA]

#define POWER_ON_MODE 0
#define WAKE_UP_MODE 1
void auxadc_battery_wakeup_measure_6331(U32 test_mode)
{
    U32 count = 0;
    U32 wakeup_volt = 0;
    
    if (test_mode == WAKE_UP_MODE) {
        printk("[auxadc_battery_wakeup_measure_test_6331] WAKEUP mode setting\n");
        //0.0 wakeup start setting for sleep mode
        mt6331_upmu_set_strup_auxadc_start_sel(0);
        mt6331_upmu_set_strup_auxadc_rstb_sw(1);
        mt6331_upmu_set_strup_auxadc_rstb_sel(1);
        //0:sleep mode
            //1:normal mode
        set_srclken_sw_mode();
        set_srclken_1_val(0);
        set_srclken_1_val(1);
    }
    
    while (!mt6331_upmu_get_auxadc_adc_rdy_wakeup()){
        count++;
        if (count> 100) {
            printk("[auxadc_battery_wakeup_measure_test_6331] WHILE LOOP TIMEOUT!!\n");
            break;
        }
    }
    
    if (mt6331_upmu_get_auxadc_adc_rdy_wakeup()){
        wakeup_volt = mt6331_upmu_get_auxadc_adc_out_wakeup();
        printk("[auxadc_battery_wakeup_measure_test_6331] wakeup_volt = %d\n", wakeup_volt);    
    }
    
    auxadc_priority_test_6331();    
}

void auxadc_measure_poweron_test_6331(void)
{
    auxadc_battery_wakeup_measure_6331(POWER_ON_MODE);
}

void auxadc_measure_wakeup_test_6331(void)
{
    auxadc_battery_wakeup_measure_6331(WAKE_UP_MODE);
}

void auxadc_battery_wakeup_measure_6332(U32 test_mode)
{
    U32 count = 0;
    U32 wakeup_volt = 0;
    
    if (test_mode == WAKE_UP_MODE) {
        printk("[auxadc_battery_wakeup_measure_test_6332] WAKEUP mode setting\n");
        //0.0 wakeup start setting for sleep mode
        mt6332_upmu_set_strup_auxadc_start_sel(0);
        mt6332_upmu_set_strup_auxadc_rstb_sw(1);
        mt6332_upmu_set_strup_auxadc_rstb_sel(1);
        //0:sleep mode
            //1:normal mode
        set_srclken_sw_mode();
        set_srclken_1_val(0);
        set_srclken_1_val(1);
    }
    
    while (!mt6332_upmu_get_auxadc_adc_rdy_wakeup()){
        count++;
        if (count> 100) {
            printk("[auxadc_battery_wakeup_measure_test_6332] WHILE LOOP TIMEOUT!!\n");
            break;
        }
    }
    
    if (mt6332_upmu_get_auxadc_adc_rdy_wakeup()){
        wakeup_volt = mt6332_upmu_get_auxadc_adc_out_wakeup();
        printk("[auxadc_battery_wakeup_measure_test_6332] wakeup_volt = %d\n", wakeup_volt);    
    }
    
    auxadc_priority_test_6332();    
}

void auxadc_measure_poweron_test_6332(void)
{
    auxadc_battery_wakeup_measure_6332(POWER_ON_MODE);
}

void auxadc_measure_wakeup_test_6332(void)
{
    auxadc_battery_wakeup_measure_6332(WAKE_UP_MODE);
}




//SWCTRL1.1. 1 channel voltage measurement in SW mode [DVT]
void auxadc_swctrl_measure_test_6331(void)
{
    U32 i, adc_output = 0;
    int channel_num[MAX_CHANNEL_NUM];
        upmu_adc_chip_list_enum chip_num[MAX_CHANNEL_NUM];
        //upmu_adc_user_list_enum user_num[MAX_CHANNEL_NUM];
            
    mt6331_upmu_set_auxadc_swctrl_en(1);
    
    for (i=0; i< MAX_CHANNEL_NUM; i++) {
        channel_num[i] = PMIC_IMM_GetChannelNumber( eChannelEnumList[i] );
        chip_num[i] = PMIC_IMM_GetChipNumber( eChannelEnumList[i] );
        //user_num[i] = PMIC_IMM_GetUserNumber(eChannelEnumList[i]);
        
        printk("[auxadc_swctrl_measure_test_6331] adc info: channel_num= %d, chip_num = %d\n", channel_num[i], chip_num[i]);
        if (chip_num[i] == MT6331_CHIP) {
            mt6331_upmu_set_auxadc_chsel(channel_num[i]);
            
            adc_output = PMIC_IMM_GetOneChannelValue(eChannelEnumList[i], 1, 0);    
            
            printk("[auxadc_swctrl_measure_test_6331] adc_result = %d\n", adc_output);
        }
    }
}


void auxadc_swctrl_measure_test_6332(void)
{
    U32 i, adc_output = 0;
    int channel_num[MAX_CHANNEL_NUM];
        upmu_adc_chip_list_enum chip_num[MAX_CHANNEL_NUM];
        //upmu_adc_user_list_enum user_num[MAX_CHANNEL_NUM];
            
    mt6332_upmu_set_auxadc_swctrl_en(1);
    
    for (i=0; i< MAX_CHANNEL_NUM; i++) {
        channel_num[i] = PMIC_IMM_GetChannelNumber(eChannelEnumList[i]);
        chip_num[i] = PMIC_IMM_GetChipNumber(eChannelEnumList[i]);
        //user_num[i] = PMIC_IMM_GetUserNumber(eChannelEnumList[i]);
        printk("[auxadc_swctrl_measure_test_6332] adc info: channel_num= %d, chip_num = %d\n", channel_num[i], chip_num[i]);
        if (chip_num[i] == MT6332_CHIP || chip_num[i] == 2) {
            if (channel_num[i]== 5)
                mt6332_upmu_set_rg_adcin_chrin_en(1);
            else if (channel_num[i] == 3)
                mt6332_upmu_set_auxadc_adcin_baton_tdet_en(1);
            mt6332_upmu_set_auxadc_chsel(channel_num[i]);
            
            adc_output = PMIC_IMM_GetOneChannelValue(eChannelEnumList[i], 1, 0);    
            
            printk("[auxadc_swctrl_measure_test_6332] adc_result = %d\n", adc_output);
        }
    }
}

//ACC1.1.1 accdet auto sampling
void auxadc_accdet_auto_sampling_test(void)
{
    mt6331_upmu_set_auxadc_accdet_auto_spl(1);
    PMIC_IMM_GetOneChannelValue(ADC_VACCDET_AP, 1, 0);
}

#endif

void pmic_dvt_entry(int test_id)
{
    printk("[pmic_dvt_entry] start : test_id=%d\n", test_id);

    switch (test_id)
    {
        //TOP-REG-CHECK
        case 1: top_6331_read();        break;
        case 2: top_6331_write(0x5a5a); break;
        case 3: top_6331_write(0xa5a5); break;
        case 4: top_6332_read();        break;
        case 5: top_6332_write(0x5a5a); break;
        case 6: top_6332_write(0xa5a5); break;
        
        // BIF
        case 1000: tc_bif_1000(); break;
        case 1001: tc_bif_1001(); break;
        case 1002: tc_bif_1002(); break;
        case 1003: tc_bif_1003(); break;
        case 1004: tc_bif_1004(); break;
        case 1005: tc_bif_1005(); break;
        case 1006: tc_bif_1006(); break;
        case 1007: tc_bif_1007(); break; //EINT
        case 1008: tc_bif_1008(); break; //EINT

        //BUCK
        case 2000: PMIC_BUCK_ON_OFF(VDVFS11_INDEX); break;
        case 2001: PMIC_BUCK_ON_OFF(VDVFS12_INDEX); break;
        case 2002: PMIC_BUCK_ON_OFF(VDVFS13_INDEX); break;
        case 2003: PMIC_BUCK_ON_OFF(VDVFS14_INDEX); break;
        case 2004: PMIC_BUCK_ON_OFF(VGPU_INDEX);    break;
        case 2005: PMIC_BUCK_ON_OFF(VCORE1_INDEX);  break;
        case 2006: PMIC_BUCK_ON_OFF(VCORE2_INDEX);  break;
        case 2007: PMIC_BUCK_ON_OFF(VIO18_INDEX);   break;
        case 2008: PMIC_BUCK_ON_OFF(VDRAM_INDEX);   break;
        case 2009: PMIC_BUCK_ON_OFF(VDVFS2_INDEX);  break;
        case 2010: PMIC_BUCK_ON_OFF(VRF1_INDEX);    break;
        case 2011: PMIC_BUCK_ON_OFF(VRF2_INDEX);    break;
        case 2012: PMIC_BUCK_ON_OFF(VPA_INDEX);     break;
        case 2013: PMIC_BUCK_ON_OFF(VSBST_INDEX);   break;

        case 2014: PMIC_BUCK_VOSEL(VDVFS11_INDEX); break;
        case 2015: PMIC_BUCK_VOSEL(VDVFS12_INDEX); break;
        case 2016: PMIC_BUCK_VOSEL(VDVFS13_INDEX); break;
        case 2017: PMIC_BUCK_VOSEL(VDVFS14_INDEX); break;
        case 2018: PMIC_BUCK_VOSEL(VGPU_INDEX);    break;
        case 2019: PMIC_BUCK_VOSEL(VCORE1_INDEX);  break;
        case 2020: PMIC_BUCK_VOSEL(VCORE2_INDEX);  break;
        case 2021: PMIC_BUCK_VOSEL(VIO18_INDEX);   break;
        case 2022: PMIC_BUCK_VOSEL(VDRAM_INDEX);   break;
        case 2023: PMIC_BUCK_VOSEL(VDVFS2_INDEX);  break;
        case 2024: PMIC_BUCK_VOSEL(VRF1_INDEX);    break;
        case 2025: PMIC_BUCK_VOSEL(VRF2_INDEX);    break;
        case 2026: PMIC_BUCK_VOSEL(VPA_INDEX);     break;
        case 2027: PMIC_BUCK_VOSEL(VSBST_INDEX);   break;

        case 2028: PMIC_BUCK_DLC(VDVFS11_INDEX); break;
        case 2029: PMIC_BUCK_DLC(VDVFS12_INDEX); break;
        case 2030: PMIC_BUCK_DLC(VDVFS13_INDEX); break;
        case 2031: PMIC_BUCK_DLC(VDVFS14_INDEX); break;
        case 2032: PMIC_BUCK_DLC(VGPU_INDEX);    break;
        case 2033: PMIC_BUCK_DLC(VCORE1_INDEX);  break;
        case 2034: PMIC_BUCK_DLC(VCORE2_INDEX);  break;
        case 2035: PMIC_BUCK_DLC(VIO18_INDEX);   break;
        case 2036: PMIC_BUCK_DLC(VDRAM_INDEX);   break;
        case 2037: PMIC_BUCK_DLC(VDVFS2_INDEX);  break;
        case 2038: PMIC_BUCK_DLC(VRF1_INDEX);    break;
        case 2039: PMIC_BUCK_DLC(VRF2_INDEX);    break;
        case 2040: PMIC_BUCK_DLC(VPA_INDEX);     break;
        case 2041: PMIC_BUCK_DLC(VSBST_INDEX);   break;

        case 2042: PMIC_BUCK_BURST(VDVFS11_INDEX); break;
        case 2043: PMIC_BUCK_BURST(VDVFS12_INDEX); break;
        case 2044: PMIC_BUCK_BURST(VDVFS13_INDEX); break;
        case 2045: PMIC_BUCK_BURST(VDVFS14_INDEX); break;
        case 2046: PMIC_BUCK_BURST(VGPU_INDEX);    break;
        case 2047: PMIC_BUCK_BURST(VCORE1_INDEX);  break;
        case 2048: PMIC_BUCK_BURST(VCORE2_INDEX);  break;
        case 2049: PMIC_BUCK_BURST(VIO18_INDEX);   break;
        case 2050: PMIC_BUCK_BURST(VDRAM_INDEX);   break;
        case 2051: PMIC_BUCK_BURST(VDVFS2_INDEX);  break;
        case 2052: PMIC_BUCK_BURST(VRF1_INDEX);    break;
        case 2053: PMIC_BUCK_BURST(VRF2_INDEX);    break;
        case 2054: PMIC_BUCK_BURST(VPA_INDEX);     break;
        case 2055: PMIC_BUCK_BURST(VSBST_INDEX);   break;
        
        //LDO        
        case 3000: exec_6331_ldo_vtcxo1_en_test();      break;
        case 3001: exec_6331_ldo_vtcxo2_en_test();      break;
        case 3002: exec_6331_ldo_vaud32_en_test();      break;
        case 3003: exec_6331_ldo_vauxa32_en_test();     break;
        case 3004: exec_6331_ldo_vcama_en_test();       break;
        case 3005: exec_6331_ldo_vmch_en_test();        break;
        case 3006: exec_6331_ldo_vemc33_en_test();      break;
        case 3007: exec_6331_ldo_vio28_en_test();       break;
        case 3008: exec_6331_ldo_vmc_en_test();         break;
        case 3009: exec_6331_ldo_vcam_af_en_test();     break;
        case 3010: exec_6331_ldo_vgp1_en_test();        break;
        case 3011: exec_6331_ldo_vgp4_en_test();        break;
        case 3012: exec_6331_ldo_vsim1_en_test();       break;
        case 3013: exec_6331_ldo_vsim2_en_test();       break;
        case 3014: exec_6331_ldo_vfbb_en_test();        break;
        case 3015: exec_6331_ldo_vrtc_en_test();        break;
        case 3016: exec_6331_ldo_vmipi_en_test();       break;
        case 3017: exec_6331_ldo_vibr_en_test();        break;
        case 3018: exec_6331_ldo_vdig18_en_test();      break;
        case 3019: exec_6331_ldo_vcamd_en_test();       break;
        case 3020: exec_6331_ldo_vusb10_en_test();      break;
        case 3021: exec_6331_ldo_vcam_io_en_test();     break;
        case 3022: exec_6331_ldo_vsram_dvfs1_en_test(); break;
        case 3023: exec_6331_ldo_vgp2_en_test();        break;
        case 3024: exec_6331_ldo_vgp3_en_test();        break;
        case 3025: exec_6331_ldo_vbiasn_en_test();      break;
        case 3026: exec_6332_ldo_vbif28_en_test();      break;
        case 3027: exec_6332_ldo_vauxb32_en_test();     break;
        case 3028: exec_6332_ldo_vusb33_en_test();      break;
        case 3029: exec_6332_ldo_vdig18_en_test();      break;
        case 3030: exec_6332_ldo_vsram_dvfs2_en_test(); break;
        
        case 3031: exec_6331_ldo_vtcxo1_vosel_test();      break;
        case 3032: exec_6331_ldo_vtcxo2_vosel_test();      break;
        case 3033: exec_6331_ldo_vaud32_vosel_test();      break;
        case 3034: exec_6331_ldo_vauxa32_vosel_test();     break;
        case 3035: exec_6331_ldo_vcama_vosel_test();       break;
        case 3036: exec_6331_ldo_vmch_vosel_test();        break;
        case 3037: exec_6331_ldo_vemc33_vosel_test();      break;
        case 3038: exec_6331_ldo_vio28_vosel_test();       break;
        case 3039: exec_6331_ldo_vmc_vosel_test();         break;
        case 3040: exec_6331_ldo_vcam_af_vosel_test();     break;
        case 3041: exec_6331_ldo_vgp1_vosel_test();        break;
        case 3042: exec_6331_ldo_vgp4_vosel_test();        break;
        case 3043: exec_6331_ldo_vsim1_vosel_test();       break;
        case 3044: exec_6331_ldo_vsim2_vosel_test();       break;
        case 3045: exec_6331_ldo_vfbb_vosel_test();        break;
        case 3046: exec_6331_ldo_vrtc_vosel_test();        break;
        case 3047: exec_6331_ldo_vmipi_vosel_test();       break;
        case 3048: exec_6331_ldo_vibr_vosel_test();        break;
        case 3049: exec_6331_ldo_vdig18_vosel_test();      break;
        case 3050: exec_6331_ldo_vcamd_vosel_test();       break;
        case 3051: exec_6331_ldo_vusb10_vosel_test();      break;
        case 3052: exec_6331_ldo_vcam_io_vosel_test();     break;
        case 3053: exec_6331_ldo_vsram_dvfs1_vosel_test(); break;
        case 3054: exec_6331_ldo_vgp2_vosel_test();        break;
        case 3055: exec_6331_ldo_vgp3_vosel_test();        break;
        case 3056: exec_6331_ldo_vbiasn_vosel_test();      break;
        case 3057: exec_6332_ldo_vbif28_vosel_test();      break;
        case 3058: exec_6332_ldo_vauxb32_vosel_test();     break;
        case 3059: exec_6332_ldo_vusb33_vosel_test();      break;
        case 3060: exec_6332_ldo_vdig18_vosel_test();      break;
        case 3061: exec_6332_ldo_vsram_dvfs2_vosel_test(); break;

        //31 vsram_dvfs1, vfbb, vrtc, vdig18 : no cal
        //32 vsram_dfvs2, dig18 : no cal
        case 3062: exec_6331_ldo_vtcxo1_cal_test();      break;
        case 3063: exec_6331_ldo_vtcxo2_cal_test();      break;
        case 3064: exec_6331_ldo_vaud32_cal_test();      break;
        case 3065: exec_6331_ldo_vauxa32_cal_test();     break;
        case 3066: exec_6331_ldo_vcama_cal_test();       break;
        case 3067: exec_6331_ldo_vmch_cal_test();        break;
        case 3068: exec_6331_ldo_vemc33_cal_test();      break;
        case 3069: exec_6331_ldo_vio28_cal_test();       break;
        case 3070: exec_6331_ldo_vmc_cal_test();         break;
        case 3071: exec_6331_ldo_vcam_af_cal_test();     break;
        case 3072: exec_6331_ldo_vgp1_cal_test();        break;
        case 3073: exec_6331_ldo_vgp4_cal_test();        break;
        case 3074: exec_6331_ldo_vsim1_cal_test();       break;
        case 3075: exec_6331_ldo_vsim2_cal_test();       break;
        case 3076: exec_6331_ldo_vfbb_cal_test();        break;
        case 3077: exec_6331_ldo_vrtc_cal_test();        break;
        case 3078: exec_6331_ldo_vmipi_cal_test();       break;
        case 3079: exec_6331_ldo_vibr_cal_test();        break;
        case 3080: exec_6331_ldo_vdig18_cal_test();      break;
        case 3081: exec_6331_ldo_vcamd_cal_test();       break;
        case 3082: exec_6331_ldo_vusb10_cal_test();      break;
        case 3083: exec_6331_ldo_vcam_io_cal_test();     break;
        case 3084: exec_6331_ldo_vsram_dvfs1_cal_test(); break;
        case 3085: exec_6331_ldo_vgp2_cal_test();        break;
        case 3086: exec_6331_ldo_vgp3_cal_test();        break;
        case 3087: exec_6331_ldo_vbiasn_cal_test();      break;
        case 3088: exec_6332_ldo_vbif28_cal_test();      break;
        case 3089: exec_6332_ldo_vauxb32_cal_test();     break;
        case 3090: exec_6332_ldo_vusb33_cal_test();      break;
        case 3091: exec_6332_ldo_vdig18_cal_test();      break;
        case 3092: exec_6332_ldo_vsram_dvfs2_cal_test(); break;

        case 3093: exec_6331_ldo_vtcxo1_mode_test();      break;
        case 3094: exec_6331_ldo_vtcxo2_mode_test();      break;
        case 3095: exec_6331_ldo_vaud32_mode_test();      break;
        case 3096: exec_6331_ldo_vauxa32_mode_test();     break;
        case 3097: exec_6331_ldo_vcama_mode_test();       break;
        case 3098: exec_6331_ldo_vmch_mode_test();        break;
        case 3099: exec_6331_ldo_vemc33_mode_test();      break;
        case 3100: exec_6331_ldo_vio28_mode_test();       break;
        case 3101: exec_6331_ldo_vmc_mode_test();         break;
        case 3102: exec_6331_ldo_vcam_af_mode_test();     break;
        case 3103: exec_6331_ldo_vgp1_mode_test();        break;
        case 3104: exec_6331_ldo_vgp4_mode_test();        break;
        case 3105: exec_6331_ldo_vsim1_mode_test();       break;
        case 3106: exec_6331_ldo_vsim2_mode_test();       break;
        case 3107: exec_6331_ldo_vfbb_mode_test();        break;
        case 3108: exec_6331_ldo_vrtc_mode_test();        break;
        case 3109: exec_6331_ldo_vmipi_mode_test();       break;
        case 3110: exec_6331_ldo_vibr_mode_test();        break;
        case 3111: exec_6331_ldo_vdig18_mode_test();      break;
        case 3112: exec_6331_ldo_vcamd_mode_test();       break;
        case 3113: exec_6331_ldo_vusb10_mode_test();      break;
        case 3114: exec_6331_ldo_vcam_io_mode_test();     break;
        case 3115: exec_6331_ldo_vsram_dvfs1_mode_test(); break;
        case 3116: exec_6331_ldo_vgp2_mode_test();        break;
        case 3117: exec_6331_ldo_vgp3_mode_test();        break;
        case 3118: exec_6331_ldo_vbiasn_mode_test();      break;
        case 3119: exec_6332_ldo_vbif28_mode_test();      break;
        case 3120: exec_6332_ldo_vauxb32_mode_test();     break;
        case 3121: exec_6332_ldo_vusb33_mode_test();      break;
        case 3122: exec_6332_ldo_vdig18_mode_test();      break;
        case 3123: exec_6332_ldo_vsram_dvfs2_mode_test(); break;

        //EFUSE
        case 4001: exec_6331_efuse_test(); break;
        case 4002: exec_6332_efuse_test(); break;

        //Interrupt
                
        //AUXADC
        case 6000: auxadc_request_test();            break;
    
    case 6001: auxadc_trimm_channel_test();            break;
    
    case 6002: auxadc_priority_test_6331();            break;
    case 6003: auxadc_priority_test_6332();            break;
    case 6004: auxadc_priority_test_GPSMD1_6331();        break;
    case 6005: auxadc_priority_test_GPSMD2_6331();        break;
    case 6006: auxadc_priority_test_GPSMD3_6331();        break;
    
    case 6007: auxadc_low_battery_int_test_6331();        break;
    case 6008: auxadc_high_battery_int_test_6331();        break;
    case 6009: auxadc_low_battery_int2_test_6331();        break;
    case 6010: auxadc_high_battery_int2_test_6331();    break;
    case 6011: auxadc_low_battery_int_test_6332();        break;
    case 6012: auxadc_high_battery_int_test_6332();        break;
    case 6013: auxadc_low_battery_int2_test_6332();        break;
    case 6014: auxadc_high_battery_int2_test_6332();    break;
    
    case 6015: auxadc_low_thermal_int_test_6331();        break;
    case 6016: auxadc_high_thermal_int_test_6331();        break;
    case 6017: auxadc_low_thermal_int2_test_6331();        break;
    case 6018: auxadc_high_thermal_int2_test_6331();    break;
    case 6019: auxadc_low_thermal_int_test_6332();        break;
    case 6020: auxadc_high_thermal_int_test_6332();        break;
    case 6021: auxadc_low_thermal_int2_test_6332();        break;
    case 6022: auxadc_high_thermal_int2_test_6332();    break;
    
    case 6023: auxadc_measure_poweron_test_6331();        break;
    case 6024: auxadc_measure_wakeup_test_6331();        break;
    case 6025: auxadc_measure_poweron_test_6332();        break;
    case 6026: auxadc_measure_wakeup_test_6332();        break;
    
    case 6027: auxadc_swctrl_measure_test_6331();        break;
    case 6028: auxadc_swctrl_measure_test_6332();        break;
    
    case 6029: auxadc_accdet_auto_sampling_test();        break;
    
    case 6030: auxadc_request_one_channel(0);        break;
    case 6031: auxadc_request_one_channel(1);        break;
    case 6032: auxadc_request_one_channel(2);        break;
    case 6033: auxadc_request_one_channel(3);        break;
    case 6034: auxadc_request_one_channel(4);        break;
    case 6035: auxadc_request_one_channel(5);        break;
    case 6036: auxadc_request_one_channel(6);        break;
    case 6037: auxadc_request_one_channel(7);        break;
    case 6038: auxadc_request_one_channel(8);        break;
    case 6039: auxadc_request_one_channel(9);        break;
    case 6040: auxadc_request_one_channel(10);        break;
    case 6041: auxadc_request_one_channel(11);        break;
    case 6042: auxadc_request_one_channel(12);        break;
    case 6043: auxadc_request_one_channel(13);        break;
    case 6044: auxadc_request_one_channel(14);        break;
    case 6045: auxadc_request_one_channel(15);        break;
    case 6046: auxadc_request_one_channel(16);        break;
    case 6047: auxadc_request_one_channel(17);        break;
    case 6048: auxadc_request_one_channel(18);        break;
    case 6049: auxadc_request_one_channel(19);        break;
    case 6050: auxadc_request_one_channel(20);        break;
    case 6051: auxadc_request_one_channel(21);        break;
        default:
            printk("[pmic_dvt_entry] test_id=%d\n", test_id);
            break;
    }

    printk("[pmic_dvt_entry] end\n");
}
