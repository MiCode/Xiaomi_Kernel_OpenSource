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

#include <asm/uaccess.h>

#include <mach/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_gpio.h>
#include <mach/mtk_rtc.h>
#include <mach/mt_spm_mtcmos.h>

#include <mach/battery_common.h>
#include <linux/time.h>

// ============================================================ //
//extern function
// ============================================================ //
extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
extern unsigned int get_pmic_mt6332_cid(void);

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)

int hw_charging_get_charger_type(void)
{
    return STANDARD_HOST;
}

#else

static void hw_bc12_dump_register(void)
{
    kal_uint32 reg_val = 0;
    kal_uint32 i = MT6332_CORE_CON0;

    reg_val = upmu_get_reg_value(i);
    battery_xlog_printk(BAT_LOG_FULL, "Reg[0x%x]=0x%x \n", i, reg_val);
}

static void hw_bc12_init(void)
{
    msleep(300);
    Charger_Detect_Init();
        
    //RG_bc12_BIAS_EN=1    
    mt6332_upmu_set_rg_bc12_bias_en(0x1);
    //RG_bc12_VSRC_EN[1:0]=00
    mt6332_upmu_set_rg_bc12_vsrc_en(0x0);
    //RG_bc12_VREF_VTH = [1:0]=00
    mt6332_upmu_set_rg_bc12_vref_vth(0x0);
    //RG_bc12_CMP_EN[1.0] = 00
    mt6332_upmu_set_rg_bc12_cmp_en(0x0);
    //RG_bc12_IPU_EN[1.0] = 00
    mt6332_upmu_set_rg_bc12_ipu_en(0x0);
    //RG_bc12_IPD_EN[1.0] = 00
    mt6332_upmu_set_rg_bc12_ipd_en(0x0);
    //bc12_RST=1
    mt6332_upmu_set_rg_bc12_rst(0x1);
    //bc12_BB_CTRL=1
    mt6332_upmu_set_rg_bc12_bb_ctrl(0x1);

    //msleep(10);
    msleep(50);
    
    if(Enable_BATDRV_LOG == BAT_LOG_FULL)
    {
        battery_xlog_printk(BAT_LOG_FULL, "hw_bc12_init() \r\n");
        hw_bc12_dump_register();
    }    

}
 
 
static U32 hw_bc12_DCD(void)
{
    U32 wChargerAvail = 0;

    //RG_bc12_IPU_EN[1.0] = 10
    if(get_pmic_mt6332_cid()==PMIC6332_E1_CID_CODE) mt6332_upmu_set_rg_bc12_ipu_en(0x1);
    else                                            mt6332_upmu_set_rg_bc12_ipu_en(0x2);
    
    //RG_bc12_IPD_EN[1.0] = 01
    if(get_pmic_mt6332_cid()==PMIC6332_E1_CID_CODE) mt6332_upmu_set_rg_bc12_ipd_en(0x2);
    else                                            mt6332_upmu_set_rg_bc12_ipd_en(0x1); 
    
    //RG_bc12_VREF_VTH = [1:0]=01
    mt6332_upmu_set_rg_bc12_vref_vth(0x1);
    
    //RG_bc12_CMP_EN[1.0] = 10
    if(get_pmic_mt6332_cid()==PMIC6332_E1_CID_CODE) mt6332_upmu_set_rg_bc12_cmp_en(0x1);
    else                                            mt6332_upmu_set_rg_bc12_cmp_en(0x2);

    //msleep(20);
    msleep(200);  //80 modify longcheer_liml_2015_10_22

    wChargerAvail = mt6332_upmu_get_rgs_bc12_cmp_out();
    
    if(Enable_BATDRV_LOG == BAT_LOG_FULL)
    {
        battery_xlog_printk(BAT_LOG_FULL, "hw_bc12_DCD() \r\n");
        hw_bc12_dump_register();
    }
    
    //RG_bc12_IPU_EN[1.0] = 00
    mt6332_upmu_set_rg_bc12_ipu_en(0x0);
    //RG_bc12_IPD_EN[1.0] = 00
    mt6332_upmu_set_rg_bc12_ipd_en(0x0);
    //RG_bc12_CMP_EN[1.0] = 00
    mt6332_upmu_set_rg_bc12_cmp_en(0x0);
    //RG_bc12_VREF_VTH = [1:0]=00
    mt6332_upmu_set_rg_bc12_vref_vth(0x0);

    return wChargerAvail;
}
 
 
static U32 hw_bc12_stepA1(void)
{
   U32 wChargerAvail = 0;
     
   //RG_bc12_IPD_EN[1.0] = 01
   if(get_pmic_mt6332_cid()==PMIC6332_E1_CID_CODE) mt6332_upmu_set_rg_bc12_ipd_en(0x2);
   else                                            mt6332_upmu_set_rg_bc12_ipd_en(0x1);
   
   //RG_bc12_VREF_VTH = [1:0]=00
   mt6332_upmu_set_rg_bc12_vref_vth(0x0);
   
   //RG_bc12_CMP_EN[1.0] = 01
   if(get_pmic_mt6332_cid()==PMIC6332_E1_CID_CODE) mt6332_upmu_set_rg_bc12_cmp_en(0x2);
   else                                            mt6332_upmu_set_rg_bc12_cmp_en(0x1);

   //msleep(80);
   msleep(80);

   wChargerAvail = mt6332_upmu_get_rgs_bc12_cmp_out();

   if(Enable_BATDRV_LOG == BAT_LOG_FULL)
   {
       battery_xlog_printk(BAT_LOG_FULL, "hw_bc12_stepA1() \r\n");
       hw_bc12_dump_register();
   }

   //RG_bc12_IPD_EN[1.0] = 00
   mt6332_upmu_set_rg_bc12_ipd_en(0x0);
   //RG_bc12_CMP_EN[1.0] = 00
   mt6332_upmu_set_rg_bc12_cmp_en(0x0);

   return  wChargerAvail;
}
 
 
static U32 hw_bc12_stepA2(void)
{
   U32 wChargerAvail = 0;
     
   //RG_bc12_VSRC_EN[1.0] = 10 
   if(get_pmic_mt6332_cid()==PMIC6332_E1_CID_CODE) mt6332_upmu_set_rg_bc12_vsrc_en(0x1);
   else                                            mt6332_upmu_set_rg_bc12_vsrc_en(0x2);
   
   //RG_bc12_IPD_EN[1:0] = 01
   if(get_pmic_mt6332_cid()==PMIC6332_E1_CID_CODE) mt6332_upmu_set_rg_bc12_ipd_en(0x2);
   else                                            mt6332_upmu_set_rg_bc12_ipd_en(0x1);
   
   //RG_bc12_VREF_VTH = [1:0]=00
   mt6332_upmu_set_rg_bc12_vref_vth(0x0);
   
   //RG_bc12_CMP_EN[1.0] = 01
   if(get_pmic_mt6332_cid()==PMIC6332_E1_CID_CODE) mt6332_upmu_set_rg_bc12_cmp_en(0x2);
   else                                            mt6332_upmu_set_rg_bc12_cmp_en(0x1);

   //msleep(80);
   msleep(80);

   wChargerAvail = mt6332_upmu_get_rgs_bc12_cmp_out();

   if(Enable_BATDRV_LOG == BAT_LOG_FULL)
   {
       battery_xlog_printk(BAT_LOG_FULL, "hw_bc12_stepA2() \r\n");
       hw_bc12_dump_register();
   }

   //RG_bc12_VSRC_EN[1:0]=00
   mt6332_upmu_set_rg_bc12_vsrc_en(0x0);
   //RG_bc12_IPD_EN[1.0] = 00
   mt6332_upmu_set_rg_bc12_ipd_en(0x0);
   //RG_bc12_CMP_EN[1.0] = 00
   mt6332_upmu_set_rg_bc12_cmp_en(0x0);

   return  wChargerAvail;
}
 
 
static U32 hw_bc12_stepB2(void)
{
   U32 wChargerAvail = 0;

   //RG_bc12_IPU_EN[1:0]=10
   if(get_pmic_mt6332_cid()==PMIC6332_E1_CID_CODE) mt6332_upmu_set_rg_bc12_ipu_en(0x1);
   else                                            mt6332_upmu_set_rg_bc12_ipu_en(0x2);
   
   //RG_bc12_VREF_VTH = [1:0]=01
   mt6332_upmu_set_rg_bc12_vref_vth(0x1);
   
   //RG_bc12_CMP_EN[1.0] = 01
   if(get_pmic_mt6332_cid()==PMIC6332_E1_CID_CODE) mt6332_upmu_set_rg_bc12_cmp_en(0x2);
   else                                            mt6332_upmu_set_rg_bc12_cmp_en(0x1);

   //msleep(80);
   msleep(80);

   wChargerAvail = mt6332_upmu_get_rgs_bc12_cmp_out();

   if(Enable_BATDRV_LOG == BAT_LOG_FULL)
   {
       battery_xlog_printk(BAT_LOG_FULL, "hw_bc12_stepB2() \r\n");
       hw_bc12_dump_register();
   }

   //RG_bc12_IPU_EN[1.0] = 00
   mt6332_upmu_set_rg_bc12_ipu_en(0x0);
   //RG_bc12_CMP_EN[1.0] = 00
   mt6332_upmu_set_rg_bc12_cmp_en(0x0);
   //RG_bc12_VREF_VTH = [1:0]=00
   mt6332_upmu_set_rg_bc12_vref_vth(0x0);

   return  wChargerAvail;
}
 
 
static void hw_bc12_done(void)
{
   //RG_bc12_VSRC_EN[1:0]=00
   mt6332_upmu_set_rg_bc12_vsrc_en(0x0);
   //RG_bc12_VREF_VTH = [1:0]=0
   mt6332_upmu_set_rg_bc12_vref_vth(0x0);
   //RG_bc12_CMP_EN[1.0] = 00
   mt6332_upmu_set_rg_bc12_cmp_en(0x0);
   //RG_bc12_IPU_EN[1.0] = 00
   mt6332_upmu_set_rg_bc12_ipu_en(0x0);
   //RG_bc12_IPD_EN[1.0] = 00
   mt6332_upmu_set_rg_bc12_ipd_en(0x0);
   //RG_bc12_BIAS_EN=0
   mt6332_upmu_set_rg_bc12_bias_en(0x0); 

   Charger_Detect_Release();

   if(Enable_BATDRV_LOG == BAT_LOG_FULL)
   {
       battery_xlog_printk(BAT_LOG_FULL, "hw_bc12_done() \r\n");
       hw_bc12_dump_register();
   }
   
}
int FG_charging_type=CHARGER_UNKNOWN;
int hw_charging_get_charger_type(void)
{
#if 0
    return STANDARD_HOST;
    //return STANDARD_CHARGER; //adaptor
#else
    CHARGER_TYPE CHR_Type_num = CHARGER_UNKNOWN;
    
    /********* Step initial  ***************/         
    hw_bc12_init();
 
    /********* Step DCD ***************/  
    if(1 == hw_bc12_DCD())
    {
         /********* Step A1 ***************/
         if(1 == hw_bc12_stepA1())
         {             
             CHR_Type_num = APPLE_2_1A_CHARGER;
             battery_xlog_printk(BAT_LOG_CRTI, "step A1 : Apple 2.1A CHARGER!\r\n");
         }
         else
         {
             CHR_Type_num = NONSTANDARD_CHARGER;
             battery_xlog_printk(BAT_LOG_CRTI, "step A1 : Non STANDARD CHARGER!\r\n");
         }
    }
    else
    {
         /********* Step A2 ***************/
         if(1 == hw_bc12_stepA2())
         {
             /********* Step B2 ***************/
             if(1 == hw_bc12_stepB2())
             {
                 CHR_Type_num = STANDARD_CHARGER;
                 battery_xlog_printk(BAT_LOG_CRTI, "step B2 : STANDARD CHARGER!\r\n");
             }
             else
             {
                 CHR_Type_num = CHARGING_HOST;
                 battery_xlog_printk(BAT_LOG_CRTI, "step B2 :  Charging Host!\r\n");
             }
         }
         else
         {
             CHR_Type_num = STANDARD_HOST;
             battery_xlog_printk(BAT_LOG_CRTI, "step A2 : Standard USB Host!\r\n");
         }
 
    }
 
    /********* Finally setting *******************************/
    hw_bc12_done();
    FG_charging_type=CHR_Type_num;
    return CHR_Type_num;
#endif    
}
#endif
