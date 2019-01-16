#include <linux/xlog.h>
#include <linux/delay.h>
#include <mach/charging.h>
#include <mach/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mach/mt_gpio.h>
#include <mach/upmu_hw.h>
#include <mach/mt_sleep.h>
#include <mach/mt_boot.h>
#include <mach/system.h>
#include <cust_gpio_usage.h>
#include <cust_charging.h>
#include "bq24160.h"

// ============================================================ //
// Define
// ============================================================ //
#define STATUS_OK    0
#define STATUS_UNSUPPORTED    -1
#define GETARRAYNUM(array) (sizeof(array)/sizeof(array[0]))

// ============================================================ //
// Global variable
// ============================================================ //

#if defined(CONFIG_MTK_WIRELESS_CHARGER_SUPPORT)
#define WIRELESS_CHARGER_EXIST_STATE 0

    #if defined(GPIO_PWR_AVAIL_WLC)
        kal_uint32 wireless_charger_gpio_number = GPIO_PWR_AVAIL_WLC; 
    #else
        kal_uint32 wireless_charger_gpio_number = 0; 
    #endif
    
#endif

static CHARGER_TYPE g_charger_type = CHARGER_UNKNOWN;

kal_bool charging_type_det_done = KAL_TRUE;

const kal_uint32 VBAT_CV_VTH[]=
{
    BATTERY_VOLT_03_500000_V, BATTERY_VOLT_03_520000_V, BATTERY_VOLT_03_540000_V, BATTERY_VOLT_03_560000_V, BATTERY_VOLT_03_580000_V, 
    BATTERY_VOLT_03_600000_V, BATTERY_VOLT_03_620000_V, BATTERY_VOLT_03_640000_V, BATTERY_VOLT_03_660000_V, BATTERY_VOLT_03_680000_V, 
    BATTERY_VOLT_03_700000_V, BATTERY_VOLT_03_720000_V, BATTERY_VOLT_03_740000_V, BATTERY_VOLT_03_760000_V, BATTERY_VOLT_03_780000_V, 
    BATTERY_VOLT_03_800000_V, BATTERY_VOLT_03_820000_V, BATTERY_VOLT_03_840000_V, BATTERY_VOLT_03_860000_V, BATTERY_VOLT_03_880000_V,
    BATTERY_VOLT_03_900000_V, BATTERY_VOLT_03_920000_V, BATTERY_VOLT_03_940000_V, BATTERY_VOLT_03_960000_V, BATTERY_VOLT_03_980000_V, 
    BATTERY_VOLT_04_000000_V, BATTERY_VOLT_04_020000_V, BATTERY_VOLT_04_040000_V, BATTERY_VOLT_04_060000_V, BATTERY_VOLT_04_080000_V, 
    BATTERY_VOLT_04_100000_V, BATTERY_VOLT_04_120000_V, BATTERY_VOLT_04_140000_V, BATTERY_VOLT_04_160000_V, BATTERY_VOLT_04_180000_V, 
    BATTERY_VOLT_04_200000_V, BATTERY_VOLT_04_220000_V, BATTERY_VOLT_04_240000_V, BATTERY_VOLT_04_260000_V, BATTERY_VOLT_04_280000_V,
    BATTERY_VOLT_04_300000_V, BATTERY_VOLT_04_320000_V, BATTERY_VOLT_04_340000_V, BATTERY_VOLT_04_360000_V, BATTERY_VOLT_04_380000_V, 
    BATTERY_VOLT_04_400000_V, BATTERY_VOLT_04_420000_V, BATTERY_VOLT_04_440000_V, BATTERY_VOLT_04_460000_V, BATTERY_VOLT_04_480000_V,
    BATTERY_VOLT_04_500000_V, BATTERY_VOLT_04_520000_V, BATTERY_VOLT_04_540000_V, BATTERY_VOLT_04_560000_V, BATTERY_VOLT_04_580000_V,
    BATTERY_VOLT_04_600000_V, BATTERY_VOLT_04_620000_V, BATTERY_VOLT_04_640000_V, BATTERY_VOLT_04_660000_V, BATTERY_VOLT_04_680000_V,
    BATTERY_VOLT_04_700000_V, BATTERY_VOLT_04_720000_V, BATTERY_VOLT_04_740000_V, BATTERY_VOLT_04_760000_V    
};

const kal_uint32 CS_VTH[]=
{
    CHARGE_CURRENT_550_00_MA,  CHARGE_CURRENT_625_00_MA,   CHARGE_CURRENT_700_00_MA,  CHARGE_CURRENT_775_00_MA,
    CHARGE_CURRENT_850_00_MA,  CHARGE_CURRENT_925_00_MA,   CHARGE_CURRENT_1000_00_MA, CHARGE_CURRENT_1075_00_MA,
    CHARGE_CURRENT_1150_00_MA, CHARGE_CURRENT_1225_00_MA,  CHARGE_CURRENT_1300_00_MA, CHARGE_CURRENT_1375_00_MA,
    CHARGE_CURRENT_1450_00_MA, CHARGE_CURRENT_1525_00_MA,  CHARGE_CURRENT_1600_00_MA, CHARGE_CURRENT_1675_00_MA,
    CHARGE_CURRENT_1750_00_MA, CHARGE_CURRENT_1825_00_MA,  CHARGE_CURRENT_1900_00_MA, CHARGE_CURRENT_1975_00_MA,
    CHARGE_CURRENT_2050_00_MA, CHARGE_CURRENT_2125_00_MA,  CHARGE_CURRENT_2200_00_MA, CHARGE_CURRENT_2275_00_MA,
    CHARGE_CURRENT_2350_00_MA, CHARGE_CURRENT_2425_00_MA,  CHARGE_CURRENT_2500_00_MA, CHARGE_CURRENT_2575_00_MA,
    CHARGE_CURRENT_2650_00_MA, CHARGE_CURRENT_2725_00_MA,  CHARGE_CURRENT_2800_00_MA, CHARGE_CURRENT_2875_00_MA
}; 

//USB connector (USB or AC adaptor)
const kal_uint32 INPUT_CS_VTH[]=
{
    CHARGE_CURRENT_100_00_MA, CHARGE_CURRENT_150_00_MA, CHARGE_CURRENT_500_00_MA, CHARGE_CURRENT_800_00_MA, 
    CHARGE_CURRENT_900_00_MA, CHARGE_CURRENT_1500_00_MA,CHARGE_CURRENT_MAX
}; 

const kal_uint32 VCDT_HV_VTH[]=
{
    BATTERY_VOLT_04_200000_V, BATTERY_VOLT_04_250000_V, BATTERY_VOLT_04_300000_V, BATTERY_VOLT_04_350000_V,
    BATTERY_VOLT_04_400000_V, BATTERY_VOLT_04_450000_V, BATTERY_VOLT_04_500000_V, BATTERY_VOLT_04_550000_V,
    BATTERY_VOLT_04_600000_V, BATTERY_VOLT_06_000000_V, BATTERY_VOLT_06_500000_V, BATTERY_VOLT_07_000000_V,
    BATTERY_VOLT_07_500000_V, BATTERY_VOLT_08_500000_V, BATTERY_VOLT_09_500000_V, BATTERY_VOLT_10_500000_V          
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
extern bool mt_usb_is_device(void);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
extern int hw_charging_get_charger_type(void);
extern unsigned int get_pmic_mt6332_cid(void);
extern void mt_power_off(void);

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

         battery_xlog_printk(BAT_LOG_FULL, "Can't find closest level, small value first \r\n");
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

        battery_xlog_printk(BAT_LOG_FULL, "Can't find closest level, large value first \r\n");      
        return pList[number -1];
          //return CHARGE_CURRENT_0_00_MA;
    }
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


static kal_uint32 charging_hw_init(void *data)
{
    kal_uint32 status = STATUS_OK;

    battery_xlog_printk(BAT_LOG_FULL,"[charging_hw_init] From Tim\n");
    
    bq24160_config_interface(bq24160_CON0, 0x1, 0x1, 7); // wdt reset
    bq24160_config_interface(bq24160_CON0, 0x1, 0x1, 3); // set USB as default supply
    bq24160_config_interface(bq24160_CON7, 0x1, 0x3, 5); // Safty timer

    //Remove for charger type detection workaround
    //bq24160_config_interface(bq24160_CON2, 0x2, 0x7, 4); // USB current limit at 500mA
    
    bq24160_config_interface(bq24160_CON3, 0x1, 0x1, 1); // IN current limit
    bq24160_config_interface(bq24160_CON5, 0x13,0x1F,3); // ICHG to BAT
    bq24160_config_interface(bq24160_CON5, 0x3, 0x7, 0); // ITERM to BAT       
    bq24160_config_interface(bq24160_CON6, 0x3, 0x7, 3); // VINDPM_USB
    bq24160_config_interface(bq24160_CON6, 0x3, 0x7, 0); // VINDPM_IN
    bq24160_config_interface(bq24160_CON7, 0x0, 0x1, 3); // Thermal sense
           
#if defined(CONFIG_MTK_WIRELESS_CHARGER_SUPPORT)
    if(wireless_charger_gpio_number!=0)
    {
        mt_set_gpio_mode(wireless_charger_gpio_number,0); // 0:GPIO mode
        mt_set_gpio_dir(wireless_charger_gpio_number,0); // 0: input, 1: output
    }
#endif
           
    return status;
}


static kal_uint32 charging_dump_register(void *data)
{
    kal_uint32 status = STATUS_OK;

    bq24160_dump_register();
      
    return status;
}    


static kal_uint32 charging_enable(void *data)
{
    kal_uint32 status = STATUS_OK;
    kal_uint32 enable = *(kal_uint32*)(data);

    if(KAL_TRUE == enable)
    {
        bq24160_set_hz_mode(0);
        bq24160_set_ce(0);
    }
    else
    {

#if defined(CONFIG_USB_MTK_HDRC_HCD)
        if(mt_usb_is_device())
#endif             
        {
            bq24160_set_ce(1);
            bq24160_set_hz_mode(1);
        }
    }
        
    return status;
}


static kal_uint32 charging_set_cv_voltage(void *data)
{
 	kal_uint32 status = STATUS_OK;
	kal_uint16 register_value;
	
	register_value = charging_parameter_to_value(VBAT_CV_VTH, GETARRAYNUM(VBAT_CV_VTH) ,*(kal_uint32 *)(data));
	bq24160_set_vbreg(register_value); 

    return status;
}     


static kal_uint32 charging_get_current(void *data)
{
    kal_uint32 status = STATUS_OK;
    kal_uint32 array_size;
    kal_uint8 reg_value;
   
    //Get current level
    array_size = GETARRAYNUM(CS_VTH);
    bq24160_read_interface(bq24160_CON5, &reg_value, 0x1F, 3); //ICHG to BAT
    *(kal_uint32 *)data = charging_value_to_parameter(CS_VTH,array_size,reg_value);
   
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
    bq24160_set_ichrg(register_value);

    return status;
}     


static kal_uint32 charging_set_input_current(void *data)
{
    kal_uint32 status = STATUS_OK;
    kal_uint32 set_chr_current;
    kal_uint32 array_size;
    kal_uint32 register_value;

    if(*(kal_uint32 *)data > CHARGE_CURRENT_500_00_MA)
    {
        register_value = 0x5;
    }
    else
    {
        array_size = GETARRAYNUM(INPUT_CS_VTH);
        set_chr_current = bmt_find_closest_level(INPUT_CS_VTH, array_size, *(kal_uint32 *)data);
        register_value = charging_parameter_to_value(INPUT_CS_VTH, array_size ,set_chr_current);    
    }
   
    bq24160_set_iusb_limit(register_value);

    return status;
}     


static kal_uint32 charging_get_charging_status(void *data)
{
    kal_uint32 status = STATUS_OK;
    kal_uint32 ret_val;

    ret_val = bq24160_get_stat();
   
    if(ret_val == 0x5)
        *(kal_uint32 *)data = KAL_TRUE;
    else
        *(kal_uint32 *)data = KAL_FALSE;
   
    return status;
}     


static kal_uint32 charging_reset_watch_dog_timer(void *data)
{
    kal_uint32 status = STATUS_OK;

    bq24160_set_tmr_rst(1);
    
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

#if 0
    //upmu_set_baton_tdet_en(1);    
    //upmu_set_rg_baton_en(1);
    //*(kal_bool*)(data) = upmu_get_rgs_baton_undet();
    *(kal_bool*)(data) = 0; // battery exist
    battery_xlog_printk(BAT_LOG_CRTI,"[charging_get_battery_status] no HW function\n");
#else
    kal_uint32 ret=0;

    pmic_config_interface(MT6332_BATON_CON0, 0x1, MT6332_PMIC_RG_BATON_EN_MASK, MT6332_PMIC_RG_BATON_EN_SHIFT);
    pmic_config_interface(MT6332_TOP_CKPDN_CON0_CLR, 0x80C0, 0xFFFF, 0); //enable BIF clock            
    pmic_config_interface(MT6332_LDO_CON2, 0x1, MT6332_PMIC_RG_VBIF28_EN_MASK, MT6332_PMIC_RG_VBIF28_EN_SHIFT);
    
    mdelay(1);
    ret = mt6332_upmu_get_bif_bat_lost();
    if(ret == 0)
    {
        *(kal_bool*)(data) = 0; // battery exist
        battery_xlog_printk(BAT_LOG_FULL,"[charging_get_battery_status] battery exist.\n");
    }
    else
    {
        *(kal_bool*)(data) = 1; // battery NOT exist
        battery_xlog_printk(BAT_LOG_CRTI,"[charging_get_battery_status] battery NOT exist.\n");
    }
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
    battery_xlog_printk(BAT_LOG_FULL,"[charging_get_charger_det_status] CHRDET status = %d\n", val);   
#else
    //*(kal_bool*)(data) = upmu_get_rgs_chrdet();
    *(kal_bool*)(data) = 1;
    battery_xlog_printk(BAT_LOG_CRTI,"[charging_get_charger_det_status] no HW function\n");
#endif    
          
    if(val == 0)
        g_charger_type = CHARGER_UNKNOWN;
          
    return status;
}


kal_bool charging_type_detection_done(void)
{
    return charging_type_det_done;
}


static kal_uint32 charging_get_charger_type(void *data)
{
    kal_uint32 status = STATUS_OK;
#if defined(CONFIG_POWER_EXT)
    *(CHARGER_TYPE*)(data) = STANDARD_HOST;
#else
    kal_uint8 val=0;
    kal_uint8 dpdm_bit=1;
    kal_uint32 i =0;
    kal_uint32 i_timeout=10000000;
    
    #if defined(CONFIG_MTK_WIRELESS_CHARGER_SUPPORT)
    int wireless_state = 0;
    if(wireless_charger_gpio_number!=0)
    {
        wireless_state = mt_get_gpio_in(wireless_charger_gpio_number);
        if(wireless_state == WIRELESS_CHARGER_EXIST_STATE)
        {
            *(CHARGER_TYPE*)(data) = WIRELESS_CHARGER;
            battery_xlog_printk(BAT_LOG_CRTI, "WIRELESS_CHARGER!\n");
            return status;
        }
    }
    else
    {
        battery_xlog_printk(BAT_LOG_CRTI, "wireless_charger_gpio_number=%d\n", wireless_charger_gpio_number);
    }
    
    if(g_charger_type!=CHARGER_UNKNOWN && g_charger_type!=WIRELESS_CHARGER)
    {
        *(CHARGER_TYPE*)(data) = g_charger_type;
        battery_xlog_printk(BAT_LOG_CRTI, "return %d!\n", g_charger_type);
        return status;
    }
    #endif
    
    if(is_chr_det()==0)
    {
        g_charger_type = CHARGER_UNKNOWN; 
        *(CHARGER_TYPE*)(data) = CHARGER_UNKNOWN;
        battery_xlog_printk(BAT_LOG_CRTI, "[charging_get_charger_type] return CHARGER_UNKNOWN\n");
        return status;
    }
    
    charging_type_det_done = KAL_FALSE;

    if(get_pmic_mt6332_cid()==PMIC6332_E1_CID_CODE)
    {        
        msleep(300);
        Charger_Detect_Init();

        //-----------------------------------------------------
        bq24160_config_interface(bq24160_CON3, 0x1, 0x1, 0);

        dpdm_bit=1;
        while(dpdm_bit!=0)
        {           
            bq24160_read_interface(bq24160_CON3, &dpdm_bit, 0x1, 0);
            battery_xlog_printk(BAT_LOG_CRTI,"[charging_get_charger_type] bq24160_CON3[0]=%d\n", dpdm_bit);

            msleep(10);

            i++;
            if(i > i_timeout)
                break;
        }

        if(i > i_timeout)
        {
            *(CHARGER_TYPE*)(data) = STANDARD_HOST;        
            battery_xlog_printk(BAT_LOG_CRTI,"[charging_get_charger_type] timeout(%d) : step=STANDARD_HOST\n", i);
        }
        else
        {
            bq24160_read_interface(bq24160_CON2, &val, 0x7, 4);
            if(val==0)
            {
                *(CHARGER_TYPE*)(data) = STANDARD_HOST;        
                battery_xlog_printk(BAT_LOG_CRTI,"[charging_get_charger_type] E1 workaround (%d), step=STANDARD_HOST\n", val);    
            }
            else
            {
                *(CHARGER_TYPE*)(data) = STANDARD_CHARGER;        
                battery_xlog_printk(BAT_LOG_CRTI,"[charging_get_charger_type] E1 workaround (%d), step=STANDARD_CHARGER\n", val);
            }
        }
        //-----------------------------------------------------

        Charger_Detect_Release();
    }
    else
    {
        *(CHARGER_TYPE*)(data) = hw_charging_get_charger_type();
    }

    charging_type_det_done = KAL_TRUE;

    g_charger_type = *(CHARGER_TYPE*)(data);
    
#endif
    return status;
}

static kal_uint32 charging_get_is_pcm_timer_trigger(void *data)
{
    kal_uint32 status = STATUS_OK;

#if 1
    if(slp_get_wake_reason() == WR_PCM_TIMER)
        *(kal_bool*)(data) = KAL_TRUE;
    else
        *(kal_bool*)(data) = KAL_FALSE;

    battery_xlog_printk(BAT_LOG_CRTI, "slp_get_wake_reason=%d\n", slp_get_wake_reason());
#else
    *(kal_bool*)(data) = KAL_FALSE;
#endif    
       
    return status;
}

static kal_uint32 charging_set_platform_reset(void *data)
{
    kal_uint32 status = STATUS_OK;

    battery_xlog_printk(BAT_LOG_CRTI, "charging_set_platform_reset\n");
 
    arch_reset(0,NULL);
        
    return status;
}

static kal_uint32 charging_get_platfrom_boot_mode(void *data)
{
    kal_uint32 status = STATUS_OK;
  
    *(kal_uint32*)(data) = get_boot_mode();

    battery_xlog_printk(BAT_LOG_CRTI, "get_boot_mode=%d\n", get_boot_mode());
         
    return status;
}

static kal_uint32 charging_set_power_off(void *data)
{
    kal_uint32 status = STATUS_OK;
  
    battery_xlog_printk(BAT_LOG_CRTI, "charging_set_power_off=%d\n");
    mt_power_off();
         
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

static kal_uint32 (* const charging_func[CHARGING_CMD_NUMBER])(void *data)=
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


