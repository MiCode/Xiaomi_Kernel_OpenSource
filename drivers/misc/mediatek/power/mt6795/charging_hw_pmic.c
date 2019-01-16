#include <mach/charging.h>
#include <mach/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <linux/xlog.h>
#include <linux/delay.h>
#include <mach/mt_sleep.h>
#include <mach/mt_boot.h>
#include <mach/system.h>

#include "cust_battery_meter.h"
#include <cust_charging.h>
#include <cust_pmic.h>
#include <linux/wakelock.h>
#include <linux/mutex.h>
#include <mach/battery_common.h>
     
 // ============================================================ //
 //define
 // ============================================================ //
#define STATUS_OK	0
#define STATUS_UNSUPPORTED	-1
#define GETARRAYNUM(array) (sizeof(array)/sizeof(array[0]))


 // ============================================================ //
 //global variable
 // ============================================================ //
kal_bool chargin_hw_init_done = KAL_TRUE; 
kal_bool charging_type_det_done = KAL_TRUE;

const kal_uint32 VBAT_CV_VTH[]=
{
    BATTERY_VOLT_04_200000_V
};

const kal_uint32 CS_VTH[]=
{    
    CHARGE_CURRENT_450_00_MA
}; 


const kal_uint32 VCDT_HV_VTH[]=
{
     BATTERY_VOLT_04_200000_V
};

// ============================================================ //
// function prototype
// ============================================================ //
 
 
// ============================================================ //
//extern variable
// ============================================================ //
 
// ============================================================ //
//extern function
// ============================================================ //
extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
extern int PMIC_IMM_GetOneChannelValue(upmu_adc_chl_list_enum dwChannel, int deCount, int trimd);
extern int hw_charging_get_charger_type(void);
extern unsigned int get_pmic_mt6332_cid(void);
extern bool mt_usb_is_device(void);

 // ============================================================ //
kal_uint32 charging_value_to_parameter(const kal_uint32 *parameter, const kal_uint32 array_size, const kal_uint32 val)
{
    if (val < array_size)
    {
        return parameter[val];
    }
    else
    {
        battery_xlog_printk(BAT_LOG_CRTI, "Can't find the parameter \r\n");    
        return parameter[0];
    }
}

 
kal_uint32 charging_parameter_to_value(const kal_uint32 *parameter, const kal_uint32 array_size, const kal_uint32 val)
{
    kal_uint32 i;

    for(i=0;i<array_size;i++)
    {
        if (val == *(parameter + i))
        {
                return i;
        }
    }

     battery_xlog_printk(BAT_LOG_CRTI, "NO register value match \r\n");
    //TODO: ASSERT(0);    // not find the value
    return 0;
}


static kal_uint32 bmt_find_closest_level(const kal_uint32 *pList,kal_uint32 number,kal_uint32 level)
{
     kal_uint32 i;
     kal_uint32 max_value_in_last_element;
 
     if(pList[0] < pList[1])
         max_value_in_last_element = KAL_TRUE;
     else
         max_value_in_last_element = KAL_FALSE;
 
     if(max_value_in_last_element == KAL_TRUE)
     {
         for(i = (number-1); i != 0; i--)     //max value in the last element
         {
             if(pList[i] <= level)
             {
                 return pList[i];
             }      
         }

         battery_xlog_printk(BAT_LOG_CRTI, "Can't find closest level \r\n");    
         return pList[0];
         //return CHARGE_CURRENT_0_00_MA;
     }
     else
     {
         for(i = 0; i< number; i++)  // max value in the first element
         {
             if(pList[i] <= level)
             {
                 return pList[i];
             }      
         }

          battery_xlog_printk(BAT_LOG_CRTI, "Can't find closest level \r\n");
         return pList[number -1];
         //return CHARGE_CURRENT_0_00_MA;
     }
}

static kal_uint32 charging_hw_init(void *data)
{
     kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)     
#else     
     // HW not support   
#endif

    return status;
}


static kal_uint32 charging_dump_register(void *data)
{
    kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)     
#else 
    // HW not support
#endif    

    return status;
}
     

static kal_uint32 charging_enable(void *data)
{
     kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)     
#else 
    kal_uint32 enable = *(kal_uint32*)(data);

    if(KAL_TRUE == enable)
    {
        // HW not support
    }
    else
    {
        // HW not support
    }
#endif
    
    return status;
}


static kal_uint32 charging_set_cv_voltage(void *data)
{
     kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)     
#else 
    // HW not support
#endif

    return status;
}     


static kal_uint32 charging_get_current(void *data)
{
    kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)     
#else 
    // HW not support
#endif
    
    return status;
}  
  


static kal_uint32 charging_set_current(void *data)
{
     kal_uint32 status = STATUS_OK;
    kal_uint32 set_chr_current;
    kal_uint32 array_size;
    kal_uint32 register_value;
    kal_uint32 current_value = *(kal_uint32 *)data;

    array_size = GETARRAYNUM(CS_VTH);
    set_chr_current = bmt_find_closest_level(CS_VTH, array_size, current_value);
    register_value = charging_parameter_to_value(CS_VTH, array_size ,set_chr_current);

#if defined(CONFIG_MTK_FPGA)     
#else 
    // HW not support
#endif

    return status;
}     


static kal_uint32 charging_set_input_current(void *data)
{
     kal_uint32 status = STATUS_OK;
    return status;
}     

static kal_uint32 charging_get_charging_status(void *data)
{
    kal_uint32 status = STATUS_OK;
    return status;
}

static void swchr_dump_register(void)
{
    battery_xlog_printk(BAT_LOG_CRTI,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n", 
        0x8052, upmu_get_reg_value(0x8052),
        0x80E0, upmu_get_reg_value(0x80E0),
        0x807A, upmu_get_reg_value(0x807A),
        0x807E, upmu_get_reg_value(0x807E),
        0x8074, upmu_get_reg_value(0x8074),
        0x8078, upmu_get_reg_value(0x8078)
        );

    battery_xlog_printk(BAT_LOG_CRTI,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n", 
        0x803C, upmu_get_reg_value(0x803C),        
        0x804C, upmu_get_reg_value(0x804C),
        0x806C, upmu_get_reg_value(0x806C),
        0x803A, upmu_get_reg_value(0x803A),
        0x8170, upmu_get_reg_value(0x8170),
        0x8166, upmu_get_reg_value(0x8166)
        );

    battery_xlog_printk(BAT_LOG_CRTI,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n", 
        0x8080, upmu_get_reg_value(0x8080),
        0x8040, upmu_get_reg_value(0x8040),
        0x8042, upmu_get_reg_value(0x8042),
        0x8050, upmu_get_reg_value(0x8050),
        0x8036, upmu_get_reg_value(0x8036),
        0x805E, upmu_get_reg_value(0x805E)
        );

    battery_xlog_printk(BAT_LOG_CRTI,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n", 
        0x8056, upmu_get_reg_value(0x8056),
        0x80E2, upmu_get_reg_value(0x80E2),
        0x8062, upmu_get_reg_value(0x8062),
        0x8178, upmu_get_reg_value(0x8178),
        0x8054, upmu_get_reg_value(0x8054),
        0x816A, upmu_get_reg_value(0x816A)
        );
    
    battery_xlog_printk(BAT_LOG_CRTI,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n", 
        0x8174, upmu_get_reg_value(0x8174),
        0x8D36, upmu_get_reg_value(0x8D36),
        0x8084, upmu_get_reg_value(0x8084),
        0x815E, upmu_get_reg_value(0x815E),
        0x8D30, upmu_get_reg_value(0x8D30), 
        0x8D34, upmu_get_reg_value(0x8D34)
        );

    battery_xlog_printk(BAT_LOG_CRTI,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n", 
        0x8D1E, upmu_get_reg_value(0x8D1E), 
        0x8D2C, upmu_get_reg_value(0x8D2C), 
        0x816C, upmu_get_reg_value(0x816C),
        0x8082, upmu_get_reg_value(0x8082),
        0x8060, upmu_get_reg_value(0x8060),
        0x8068, upmu_get_reg_value(0x8068)
        );
}

static void mt_swchr_debug_msg(void)
{
    battery_xlog_printk(BAT_LOG_CRTI,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n", 
        0x805E, upmu_get_reg_value(0x805E),
        0x8056, upmu_get_reg_value(0x8056),
        0x80E2, upmu_get_reg_value(0x80E2),
        0x8062, upmu_get_reg_value(0x8062),
        0x8178, upmu_get_reg_value(0x8178),
        0x8054, upmu_get_reg_value(0x8054)        
        );
}

void set_cv_volt(void)
{
    kal_uint32 is_m3_en = 0;

    pmic_read_interface(0x805E, &is_m3_en, 0x1, 2); //RGS_M3_EN        
    if(is_m3_en==1)
    {
        battery_xlog_printk(BAT_LOG_FULL,"[set_cv_volt] RGS_M3_EN=%d, set CV to high\n", is_m3_en); 
    
        #if 0 // for phone
            battery_xlog_printk(BAT_LOG_CRTI,"[set_cv_volt] g_cv_reg_val=0x%x\n", g_cv_reg_val); 
            mt6332_upmu_set_rg_cv_sel(g_cv_reg_val);    
            mt6332_upmu_set_rg_cv_pp_sel(g_cv_reg_val);             
        #else
            //set CV_VTH (ex=4.2) and RG_CV_PP_SEL (ex=4.3)
            #if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)        
            //battery_xlog_printk(BAT_LOG_CRTI, "[set_cv_volt] HIGH_BATTERY_VOLTAGE_SUPPORT\n");
            mt6332_upmu_set_rg_cv_sel(0x4);    // 4.35V///liuchao05---04
            mt6332_upmu_set_rg_cv_pp_sel(0x4); // 4.35V
            #else              
            mt6332_upmu_set_rg_cv_sel(0x8);    // 4.2V
            mt6332_upmu_set_rg_cv_pp_sel(0x8); // 4.2V
            #endif
        #endif    

        //Reg[0x816A]
        pmic_config_interface(0x816A,0x1,0x1,5);
    }
    else
    {
        battery_xlog_printk(BAT_LOG_CRTI,"[set_cv_volt] RGS_M3_EN=%d, can not set CV to high\n", is_m3_en);
    }
}

void swchr_hw_init(void)
{
    set_cv_volt();
        
    //Reg[0x8074]   
    mt6332_upmu_set_rg_csbat_vsns(1);
    //Reg[0x804C]   
    mt6332_upmu_set_rg_swchr_trev(0x0);
    //Reg[0x806C]   
    mt6332_upmu_set_rg_ch_complete_det_off(0); 
    mt6332_upmu_set_rg_ch_complete_pwm_off(0);
    mt6332_upmu_set_rg_ch_complete_m3_off(1);
    //Reg[0x803A]
    mt6332_upmu_set_rg_iterm_sel(0x0);            //[15:13]
    mt6332_upmu_set_rg_ics_loop(0x0);             //[11:10]
    mt6332_upmu_set_rg_hfdet_en(0x0);             //[9]
    mt6332_upmu_set_rg_gdri_minoff_dis(0x1);      //[8]
    mt6332_upmu_set_rg_cv_comprc(0x0);            //[2:0]
    //Reg[0x8170]
    mt6332_upmu_set_rg_force_dcin_pp(0x1);        //[8]
    mt6332_upmu_set_rg_thermal_reg_mode_off(0x0); //[6]
    mt6332_upmu_set_rg_adaptive_cv_mode_off(0x1); //[5]
    mt6332_upmu_set_rg_vin_dpm_mode_off(0x0);     //[4], 1->0, Tim, 20140328
    //Reg[0x8166]
    mt6332_upmu_set_rg_ovpfet_sw_fast(1);
    mt6332_upmu_set_rg_ovpfet_sw_target(0x4);
    //Reg[0x8040]
    mt6332_upmu_set_rg_swchr_vrampcc(0x2);
    mt6332_upmu_set_rg_swchr_chrinslp(0x2);
    //Reg[0x8042]
    mt6332_upmu_set_rg_swchr_vrampslp(0xE);
    //Reg[0x8050]
    mt6332_upmu_set_rg_swchr_rccomp_tune(0x1);
    //Reg[0x8036]
    mt6332_upmu_set_rg_asw(0x1);
    mt6332_upmu_set_rg_chr_force_pwm(0x0);
    //Reg[0x815E]
    pmic_config_interface(0x815E,0xF,0xF,0); // [3:0]=0xF, Ricky, [6]=is HW happen    
}

static kal_uint32 is_chr_det(void)
{
    kal_uint32 val=0;
    pmic_config_interface(0x10A, 0x1, 0xF, 8);
    pmic_config_interface(0x10A, 0x17,0xFF,0);
    pmic_read_interface(0x108,   &val,0x1, 1);

    battery_xlog_printk(BAT_LOG_CRTI,"[is_chr_det] %d\n", val);
    
    return val;
}

void set_usb_dc_in_mode(int is_sw_en, int is_sw_mode)
{
    pmic_config_interface(0x8D1E, is_sw_en,   0x1, 4);
    pmic_config_interface(0x8D2C, is_sw_mode, 0x1, 4);
    
    pmic_config_interface(0x8D1E, is_sw_en,   0x1, 5);
    pmic_config_interface(0x8D2C, is_sw_mode, 0x1, 5);

    battery_xlog_printk(BAT_LOG_CRTI,"[set_usb_dc_in_mode] Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
        0x8D1E, upmu_get_reg_value(0x8D1E),
        0x8D2C, upmu_get_reg_value(0x8D2C)
        );
}

static void swchr_flow_normal(void)
{
    //set PRECC
    mt6332_upmu_set_rg_iprecc_swen(0);
    //turn off PRECC current
    mt6332_upmu_set_rg_precc_en_swen(1);
    mt6332_upmu_set_rg_precc_en(0);
    
    //set ICH
    mt6332_upmu_set_rg_ich_sel_swen(1);
    mt6332_upmu_set_rg_ich_sel(0x5);

    mt6332_upmu_set_rg_pwm_en(0);
}

int upmu_is_chr_det_hal(void)
{
    if(is_chr_det() == 0)
    {
        return 0;
    }
    else
    {
        if( mt_usb_is_device() )
        {
        	battery_xlog_printk(BAT_LOG_CRTI, "[upmu_is_chr_det_hal] Charger exist and USB is not host\n");
            return 1;
        }
        else
        {
            battery_xlog_printk(BAT_LOG_CRTI, "[upmu_is_chr_det_hal] Charger exist but USB is host\n");
            return 0;
        }
    }
}


void sw_plug_out_check(void)
{
// chr_plug_out_sw_detect    
#if 1 
    if(upmu_is_chr_det_hal()==1) //sync USB device/otg state
    {
        kal_uint32 cv_val = 0;
        kal_uint32 ich_low_val = 0;
        kal_uint32 is_charge_complete = 0;
        
        pmic_read_interface(0x805E, &cv_val, 0x1, 0);
        if(cv_val == 1)
        {
            battery_xlog_printk(BAT_LOG_FULL,"[chr_plug_out_sw_detect] in CV\n");
            pmic_config_interface(0x8074, 0x1, 0x1, 9);
            pmic_config_interface(0x8166, 0x1, 0x1,12);
            pmic_config_interface(0x8D36, 0x3, 0x3,11); //[12:11]=0x3
        }
        else
        {
            battery_xlog_printk(BAT_LOG_FULL,"[chr_plug_out_sw_detect] not in CV\n");            
            pmic_config_interface(0x8074, 0x0, 0x1, 9);
            pmic_config_interface(0x8166, 0x1, 0x1,12);
            pmic_config_interface(0x8D36, 0x0, 0x3,11); //[12:11]=0x0
        }
            
    #if 1            
            battery_xlog_printk(BAT_LOG_FULL,"[chr_plug_out_sw_detect] Reg[0x%x]=0x%x\n", 0x8074, upmu_get_reg_value(0x8074) );

            pmic_read_interface(0x8054, &ich_low_val, 0x1, 1);
            pmic_read_interface(0x805E, &is_charge_complete, 0x1, 10);
            battery_xlog_printk(BAT_LOG_CRTI,"[chr_plug_out_sw_detect] ich_low_val=%d, is_charge_complete=%d\n", 
                ich_low_val, is_charge_complete);
            
            //if( (ich_low_val==1) || (is_charge_complete==1) )
            if(is_chr_det()==1) // for evb
            {
                set_usb_dc_in_mode(0,0);                
                set_usb_dc_in_mode(0,1);                
                battery_xlog_printk(BAT_LOG_CRTI,"[chr_plug_out_sw_detect] Reg[0x%x]=0x%x\n", 0x816C, upmu_get_reg_value(0x816C) );
                //
                msleep(10);
                if(is_chr_det()==1)
                {
                    set_usb_dc_in_mode(0,0);
                }                
            }
            else
            {
                set_usb_dc_in_mode(0,0);                
            }
    #endif
            
        //debug
        swchr_dump_register();
        mt_swchr_debug_msg();

        battery_xlog_printk(BAT_LOG_FULL,"[chr_plug_out_sw_detect] Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                0x8D1E, upmu_get_reg_value(0x8D1E), 0x8D2C, upmu_get_reg_value(0x8D2C), 0x816C, upmu_get_reg_value(0x816C) );
    }
    else
    {
        battery_xlog_printk(BAT_LOG_FULL,"[chr_plug_out_sw_detect] no cable\n");
    }
#endif

}
 
static kal_uint32 charging_reset_watch_dog_timer(void *data)
{
    kal_uint32 status = STATUS_OK;
    
#if defined(CONFIG_MTK_FPGA)     
#else 
    // HW not support
#endif


#if defined(CONFIG_POWER_EXT)
    if(mt6332_upmu_get_swcid() >= PMIC6332_E3_CID_CODE)
    {
        swchr_hw_init();
        swchr_flow_normal();
        //MT6332 plug-out in EVB
        sw_plug_out_check();
    }
#endif

    return status;
}


static kal_uint32 charging_set_hv_threshold(void *data)
{
    kal_uint32 status = STATUS_OK;

    //HW Fixed value

    return status;
}


static kal_uint32 charging_get_hv_status(void *data)
{
      kal_uint32 status = STATUS_OK;
      kal_uint32 val=0;
  
#if defined(CONFIG_POWER_EXT)
      *(kal_bool*)(data) = 0;
#else
      if(get_pmic_mt6332_cid()==PMIC6332_E1_CID_CODE)
      {
          *(kal_bool*)(data) = 0;        
      }
      else
      {
          val= mt6332_upmu_get_rgs_chr_hv_det();
          *(kal_bool*)(data) = val;
      }    
#endif
  
      if(val==1)
      {
          battery_xlog_printk(BAT_LOG_CRTI,"[charging_get_hv_status] HV detected by HW (%d)\n", val);
      }
       
      return status;
}

        
static kal_uint32 charging_get_battery_status(void *data)
{
      kal_uint32 status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)     
#else
      // HW not support
#endif

    #if defined(CONFIG_POWER_EXT)
    *(kal_bool*)(data) = 0; // battery exist
    battery_xlog_printk(BAT_LOG_CRTI,"[charging_get_battery_status] EVB no HW battery\n");
    #endif

      return status;
}

    
static kal_uint32 charging_get_charger_det_status(void *data)
{
       kal_uint32 status = STATUS_OK;

#if 1
     kal_uint32 val=0;
     pmic_config_interface(0x10A, 0x1, 0xF, 8);
     pmic_config_interface(0x10A, 0x17,0xFF,0);
     pmic_read_interface(0x108,   &val,0x1, 1);
     *(kal_bool*)(data) = val;
     battery_xlog_printk(BAT_LOG_CRTI,"[charging_get_charger_det_status] CHRDET status = %d\n", val);
#else 
     //*(kal_bool*)(data) = upmu_get_rgs_chrdet();
     *(kal_bool*)(data) = 1;
     battery_xlog_printk(BAT_LOG_CRTI,"[charging_get_charger_det_status] no HW function\n");
#endif
       
       return status;
}


kal_bool charging_type_detection_done(void)
{
     return charging_type_det_done;
}


static kal_uint32 charging_get_charger_type(void *data)
{
    kal_uint32 status = STATUS_OK;
     
#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
    *(CHARGER_TYPE*)(data) = STANDARD_HOST;
#else
    *(CHARGER_TYPE*)(data) = STANDARD_HOST;
#endif

     return status;
}

static kal_uint32 charging_get_is_pcm_timer_trigger(void *data)
{
     kal_uint32 status = STATUS_OK;
     
#if defined(CONFIG_MTK_FPGA)     
#else 
     if(slp_get_wake_reason() == WR_PCM_TIMER)
         *(kal_bool*)(data) = KAL_TRUE;
     else
         *(kal_bool*)(data) = KAL_FALSE;
 
     battery_xlog_printk(BAT_LOG_CRTI, "slp_get_wake_reason=%d\n", slp_get_wake_reason());
#endif
        
     return status;
 }
 
 static kal_uint32 charging_set_platform_reset(void *data)
 {
     kal_uint32 status = STATUS_OK;
     
#if defined(CONFIG_MTK_FPGA)     
#else 
     #if !defined(CONFIG_ARM64)
     battery_xlog_printk(BAT_LOG_CRTI, "charging_set_platform_reset\n");  
     arch_reset(0,NULL);
     #else
     battery_xlog_printk(BAT_LOG_CRTI, "wait arch_reset ready\n"); 
     #endif
#endif
         
     return status;
 }
 
 static kal_uint32 charging_get_platfrom_boot_mode(void *data)
 {
     kal_uint32 status = STATUS_OK;
     
#if defined(CONFIG_MTK_FPGA)     
#else   
     *(kal_uint32*)(data) = get_boot_mode();
 
     battery_xlog_printk(BAT_LOG_CRTI, "get_boot_mode=%d\n", get_boot_mode());
#endif
          
     return status;
}

static kal_uint32 charging_set_power_off(void *data)
{
    kal_uint32 status = STATUS_OK;
  
    battery_xlog_printk(BAT_LOG_CRTI, "charging_set_power_off=\n");
    //mt_power_off();
         
    return status;
}

static kal_uint32 charging_get_power_source(void *data)
{
	return STATUS_UNSUPPORTED;
}

static kal_uint32 charging_get_csdac_full_flag(void *data)
{
	return STATUS_UNSUPPORTED;	
}

static kal_uint32 charging_set_ta_current_pattern(void *data)
{
	return STATUS_UNSUPPORTED;	
}

static kal_uint32 (* charging_func[CHARGING_CMD_NUMBER])(void *data)=
{
	charging_hw_init
	,charging_dump_register  	
	,charging_enable
	,charging_set_cv_voltage
	,charging_get_current
	,charging_set_current
	,charging_set_input_current
	,charging_get_charging_status
	,charging_reset_watch_dog_timer
	,charging_set_hv_threshold
	,charging_get_hv_status
	,charging_get_battery_status
	,charging_get_charger_det_status
	,charging_get_charger_type
	,charging_get_is_pcm_timer_trigger
	,charging_set_platform_reset
	,charging_get_platfrom_boot_mode
	,charging_set_power_off
	,charging_get_power_source
	,charging_get_csdac_full_flag
	,charging_set_ta_current_pattern
 };

 
 /*
 * FUNCTION
 *        Internal_chr_control_handler
 *
 * DESCRIPTION                                                             
 *         This function is called to set the charger hw
 *
 * CALLS  
 *
 * PARAMETERS
 *        None
 *     
 * RETURNS
 *        
 *
 * GLOBALS AFFECTED
 *       None
 */
kal_int32 chr_control_interface(CHARGING_CTRL_CMD cmd, void *data)
{
     kal_int32 status;
     if(cmd < CHARGING_CMD_NUMBER)
         status = charging_func[cmd](data);
     else
         return STATUS_UNSUPPORTED;
 
     return status;
}
