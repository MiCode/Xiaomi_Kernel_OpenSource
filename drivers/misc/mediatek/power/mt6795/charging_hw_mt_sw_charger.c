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
#include <cust_pmic.h>

// ============================================================ //
// Define
// ============================================================ //
#define STATUS_OK    0
#define STATUS_UNSUPPORTED    -1
#define GETARRAYNUM(array) (sizeof(array)/sizeof(array[0]))

// ============================================================ //
// Global variable
// ============================================================ //

int g_chr_complete_timer=0;
int g_sw_rechr_flag=0;
int g_sw_rechr_vlot=4100;
extern int Charger_enable_Flag;//add by longcheer_liml_09_02

#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)        
kal_uint32 g_cv_reg_val=0x4; //default 4.35V//liuchao//05--04
#else              
kal_uint32 g_cv_reg_val=0x8; //default 4.2V
#endif

kal_bool chargin_hw_init_done = KAL_TRUE; 

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
    BATTERY_VOLT_04_600000_V, BATTERY_VOLT_04_550000_V, BATTERY_VOLT_04_500000_V, BATTERY_VOLT_04_450000_V,
    BATTERY_VOLT_04_400000_V, BATTERY_VOLT_04_350000_V, BATTERY_VOLT_04_300000_V, BATTERY_VOLT_04_250000_V,    
    BATTERY_VOLT_04_200000_V, BATTERY_VOLT_04_150000_V, BATTERY_VOLT_04_100000_V, BATTERY_VOLT_04_050000_V,
    BATTERY_VOLT_04_000000_V, BATTERY_VOLT_03_950000_V, BATTERY_VOLT_03_900000_V, BATTERY_VOLT_03_850000_V,
    BATTERY_VOLT_03_800000_V, BATTERY_VOLT_03_750000_V, BATTERY_VOLT_03_700000_V, BATTERY_VOLT_03_650000_V,
    BATTERY_VOLT_03_600000_V, BATTERY_VOLT_03_550000_V, BATTERY_VOLT_03_500000_V, BATTERY_VOLT_03_450000_V,
    BATTERY_VOLT_03_400000_V, BATTERY_VOLT_03_350000_V, BATTERY_VOLT_03_300000_V
};

const kal_uint32 CS_VTH[]=
{
    CHARGE_CURRENT_100_00_MA,  CHARGE_CURRENT_125_00_MA,   CHARGE_CURRENT_200_00_MA,  CHARGE_CURRENT_300_00_MA,
    CHARGE_CURRENT_450_00_MA,  CHARGE_CURRENT_500_00_MA,   CHARGE_CURRENT_600_00_MA,  CHARGE_CURRENT_700_00_MA,
    CHARGE_CURRENT_800_00_MA,  CHARGE_CURRENT_900_00_MA,   CHARGE_CURRENT_1000_00_MA, CHARGE_CURRENT_1100_00_MA,
    CHARGE_CURRENT_1200_00_MA, CHARGE_CURRENT_1300_00_MA,  CHARGE_CURRENT_1400_00_MA, CHARGE_CURRENT_1500_00_MA,
    CHARGE_CURRENT_1600_00_MA, CHARGE_CURRENT_1700_00_MA,  CHARGE_CURRENT_1800_00_MA, CHARGE_CURRENT_1900_00_MA,
    CHARGE_CURRENT_2000_00_MA, CHARGE_CURRENT_2100_00_MA,  CHARGE_CURRENT_2200_00_MA, CHARGE_CURRENT_2300_00_MA,
    CHARGE_CURRENT_2400_00_MA, CHARGE_CURRENT_2500_00_MA,  CHARGE_CURRENT_2600_00_MA, CHARGE_CURRENT_2700_00_MA,
    CHARGE_CURRENT_2800_00_MA, CHARGE_CURRENT_2900_00_MA,  CHARGE_CURRENT_3000_00_MA, CHARGE_CURRENT_3100_00_MA,
    CHARGE_CURRENT_MAX
}; 

//USB connector (USB or AC adaptor)
const kal_uint32 INPUT_CS_VTH[]=
{
    CHARGE_CURRENT_100_00_MA,  CHARGE_CURRENT_125_00_MA,   CHARGE_CURRENT_200_00_MA,  CHARGE_CURRENT_300_00_MA,
    CHARGE_CURRENT_450_00_MA,  CHARGE_CURRENT_500_00_MA,   CHARGE_CURRENT_600_00_MA,  CHARGE_CURRENT_700_00_MA,
    CHARGE_CURRENT_800_00_MA,  CHARGE_CURRENT_900_00_MA,   CHARGE_CURRENT_1000_00_MA, CHARGE_CURRENT_1100_00_MA,
    CHARGE_CURRENT_1200_00_MA, CHARGE_CURRENT_1300_00_MA,  CHARGE_CURRENT_1400_00_MA, CHARGE_CURRENT_1500_00_MA,
    CHARGE_CURRENT_1600_00_MA, CHARGE_CURRENT_1700_00_MA,  CHARGE_CURRENT_1800_00_MA, CHARGE_CURRENT_1900_00_MA,
    CHARGE_CURRENT_2000_00_MA, CHARGE_CURRENT_2100_00_MA,  CHARGE_CURRENT_2200_00_MA, CHARGE_CURRENT_2300_00_MA,
    CHARGE_CURRENT_2400_00_MA, CHARGE_CURRENT_2500_00_MA,  CHARGE_CURRENT_2600_00_MA, CHARGE_CURRENT_2700_00_MA,
    CHARGE_CURRENT_2800_00_MA, CHARGE_CURRENT_2900_00_MA,  CHARGE_CURRENT_3000_00_MA, CHARGE_CURRENT_3100_00_MA,
    CHARGE_CURRENT_MAX
}; 

const kal_uint32 VCDT_HV_VTH[]=
{
    //HW Fixed value
};

static kal_uint32 charging_error = false;
static kal_uint32 g_wake_reason_is_none = 0;
// ============================================================ //
// function prototype
// ============================================================ //

static kal_uint32 charging_get_error_state(void);
static kal_uint32 charging_set_error_state(void *data);
 
// ============================================================ //
//extern variable
// ============================================================ //

// ============================================================ //
//extern function
// ============================================================ //
extern int PMIC_IMM_GetOneChannelValue(upmu_adc_chl_list_enum dwChannel, int deCount, int trimd);
extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);
extern bool mt_usb_is_device(void);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
extern int hw_charging_get_charger_type(void);
extern unsigned int get_pmic_mt6332_cid(void);
extern void mt_power_off(void);
extern kal_bool upmu_is_chr_det(void);
extern bool get_usb_current_unlimited(void);
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
    	if (*(parameter) > *(parameter+1)) {
	        if (val >= *(parameter + i))
	        {
	            if (abs(val - *(parameter + i)) < abs(val - *(parameter + i-1)))
	                return i;
				else
					return i-1;
	        } 
		} else {
			if (val <= *(parameter + i)) {
				if (abs(val - *(parameter + i)) < abs(val - *(parameter + i-1)))
	                return i;
				else
					return i-1;
			}
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
    
        battery_xlog_printk(BAT_LOG_CRTI, "Can't find closest level, small value first \r\n");
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
    
        battery_xlog_printk(BAT_LOG_CRTI, "Can't find closest level, large value first \r\n");      
        return pList[number -1];
        //return CHARGE_CURRENT_0_00_MA;
    }
}


static void ich_pre_init(void)
{
    kal_uint32 val=0;

    pmic_read_interface(0x8078,&val,0x1F,0);
    if(val==0)
    {
        //set ICH
        mt6332_upmu_set_rg_ich_sel_swen(1);

        if (g_charger_type == STANDARD_CHARGER || get_usb_current_unlimited()) 
            mt6332_upmu_set_rg_ich_sel(0xa);            
        else
            mt6332_upmu_set_rg_ich_sel(0x5);        
    }
    battery_xlog_printk(BAT_LOG_FULL,"[ich_pre_init] Reg[0x%x]=0x%x\n", 0x8078, upmu_get_reg_value(0x8078));
}

static kal_uint32 is_chr_det(void)
{
    kal_uint32 val=0;
    pmic_config_interface(0x10A, 0x1, 0xF, 8);
    pmic_config_interface(0x10A, 0x17,0xFF,0);
    pmic_read_interface(0x108,   &val,0x1, 1);

    battery_xlog_printk(BAT_LOG_CRTI,"[is_chr_det] %d\n", val);

    //ich pre-init
    ich_pre_init();
    
    return val;
}


static void swchr_dump_register(void)
{
    battery_xlog_printk(BAT_LOG_FULL,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n", 
        0x8052, upmu_get_reg_value(0x8052),
        0x80E0, upmu_get_reg_value(0x80E0),
        0x807A, upmu_get_reg_value(0x807A),
        0x807E, upmu_get_reg_value(0x807E),
        0x8074, upmu_get_reg_value(0x8074),
        0x8078, upmu_get_reg_value(0x8078)
        );

    battery_xlog_printk(BAT_LOG_FULL,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n", 
        0x803C, upmu_get_reg_value(0x803C),        
        0x804C, upmu_get_reg_value(0x804C),
        0x806C, upmu_get_reg_value(0x806C),
        0x803A, upmu_get_reg_value(0x803A),
        0x8170, upmu_get_reg_value(0x8170),
        0x8166, upmu_get_reg_value(0x8166)
        );

    battery_xlog_printk(BAT_LOG_FULL,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n", 
        0x8080, upmu_get_reg_value(0x8080),
        0x8040, upmu_get_reg_value(0x8040),
        0x8042, upmu_get_reg_value(0x8042),
        0x8050, upmu_get_reg_value(0x8050),
        0x8036, upmu_get_reg_value(0x8036),
        0x805E, upmu_get_reg_value(0x805E)
        );

    battery_xlog_printk(BAT_LOG_FULL,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n", 
        0x8056, upmu_get_reg_value(0x8056),
        0x80E2, upmu_get_reg_value(0x80E2),
        0x8062, upmu_get_reg_value(0x8062),
        0x8178, upmu_get_reg_value(0x8178),
        0x8054, upmu_get_reg_value(0x8054),
        0x816A, upmu_get_reg_value(0x816A)
        );
    
    battery_xlog_printk(BAT_LOG_FULL,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n", 
        0x8174, upmu_get_reg_value(0x8174),
        0x8D36, upmu_get_reg_value(0x8D36),
        0x8084, upmu_get_reg_value(0x8084),
        0x815E, upmu_get_reg_value(0x815E),
        0x8D30, upmu_get_reg_value(0x8D30), 
        0x8D34, upmu_get_reg_value(0x8D34)
        );

    battery_xlog_printk(BAT_LOG_FULL,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n",
        0x8D1E, upmu_get_reg_value(0x8D1E), 
        0x8D2C, upmu_get_reg_value(0x8D2C), 
        0x816C, upmu_get_reg_value(0x816C),
        0x8082, upmu_get_reg_value(0x8082),
        0x8060, upmu_get_reg_value(0x8060),
        0x8068, upmu_get_reg_value(0x8068)
        );

    battery_xlog_printk(BAT_LOG_FULL,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n",
        0x809A, upmu_get_reg_value(0x809A),
        0x804A, upmu_get_reg_value(0x804A),
        0x8D1A, upmu_get_reg_value(0x8D1A),
        0x8D28, upmu_get_reg_value(0x8D28)
        );
}

static void mt_swchr_debug_msg(void)
{
    battery_xlog_printk(BAT_LOG_CRTI,"[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x,[0x%x]=0x%x\n", 
        0x805E, upmu_get_reg_value(0x805E),
        0x8056, upmu_get_reg_value(0x8056),
        0x80E2, upmu_get_reg_value(0x80E2),
        0x8062, upmu_get_reg_value(0x8062),
        0x8078, upmu_get_reg_value(0x8078),
        0x8178, upmu_get_reg_value(0x8178),
        0x8054, upmu_get_reg_value(0x8054)
        );
}

static void check_sw_recharge(void)
{
    kal_uint32 is_charge_complete=0;
    kal_uint32 volt_vbat=0;
    kal_uint32 cv_val=0;

    if(g_sw_rechr_flag==0)
    {
        pmic_read_interface(0x805E, &is_charge_complete, 0x1, 10); //RGS_CHARGE_COMPLETE_DET
        battery_xlog_printk(BAT_LOG_FULL,"[check_sw_recharge] read RGS_CHARGE_COMPLETE_DET=%d\n", is_charge_complete); 
    }

    if(is_charge_complete==1)
    {
        g_sw_rechr_flag=1;
        g_chr_complete_timer=0;
    }

    if(g_sw_rechr_flag==1)
    {
        g_chr_complete_timer+=10;
        
        if(g_chr_complete_timer>30)
            g_chr_complete_timer=30;
    }

    if(is_chr_det()==0)
    {
        g_sw_rechr_flag=0;
        g_chr_complete_timer=0;
        pmic_config_interface(0x8D30, 0x0, 0x1, 10);
        pmic_config_interface(0x8D34, 0x0, 0x1, 10);        
    }

    //check sw_rechr_vlot
    pmic_read_interface(MT6332_CHR_CON11,&cv_val,MT6332_PMIC_RG_CV_SEL_MASK,MT6332_PMIC_RG_CV_SEL_SHIFT);
    g_sw_rechr_vlot=4600-(50*cv_val)-100;

    if( (g_chr_complete_timer>=30) && (g_sw_rechr_flag==1) )
    {
        pmic_config_interface(0x8D30, 0x1, 0x1, 10);
        pmic_config_interface(0x8D34, 0x1, 0x1, 10);        

        volt_vbat = PMIC_IMM_GetOneChannelValue(ADC_BATSNS_AP,2,1);
        if(volt_vbat<=g_sw_rechr_vlot)
        {
            g_sw_rechr_flag=0;
            g_chr_complete_timer=0;
            pmic_config_interface(0x8D30, 0x0, 0x1, 10);
            pmic_config_interface(0x8D34, 0x0, 0x1, 10);        
        }
    }

    battery_xlog_printk(BAT_LOG_FULL,"[check_sw_recharge] Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        0x8D30, upmu_get_reg_value(0x8D30), 0x8D34, upmu_get_reg_value(0x8D34) );
    battery_xlog_printk(BAT_LOG_CRTI,"[check_sw_recharge] volt_vbat=%d, g_sw_rechr_vlot=%d, g_chr_complete_timer=%d, g_sw_rechr_flag=%d\n",
        volt_vbat, g_sw_rechr_vlot, g_chr_complete_timer, g_sw_rechr_flag); 
}

void set_cv_volt(void)
{
    kal_uint32 is_m3_en = 0;

    pmic_read_interface(0x805E, &is_m3_en, 0x1, 2); //RGS_M3_EN        
    if(is_m3_en==1)
    {
        battery_xlog_printk(BAT_LOG_FULL,"[set_cv_volt] RGS_M3_EN=%d, set CV to high\n", is_m3_en); 
    
        #if 1
            battery_xlog_printk(BAT_LOG_CRTI,"[set_cv_volt] g_cv_reg_val=0x%x\n", g_cv_reg_val); 
            mt6332_upmu_set_rg_cv_sel(g_cv_reg_val);    
            mt6332_upmu_set_rg_cv_pp_sel(g_cv_reg_val);             
        #else
            //set CV_VTH (ex=4.2) and RG_CV_PP_SEL (ex=4.3)
            #if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)        
            //battery_xlog_printk(BAT_LOG_CRTI, "[set_cv_volt] HIGH_BATTERY_VOLTAGE_SUPPORT\n");
            mt6332_upmu_set_rg_cv_sel(0x4);    // 4.35V//liuchao 05--04
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
    #ifdef CONFIG_MTK_ENABLE_PP
    mt6332_upmu_set_rg_ch_complete_pwm_off(0);
    #else
    mt6332_upmu_set_rg_ch_complete_pwm_off(1);  // 1:disable PWM
    #endif 
    mt6332_upmu_set_rg_ch_complete_m3_off(0);
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

    //CY
    pmic_config_interface(0x8D1E,0x1,0x1,1);
    pmic_config_interface(0x8082,0x0,0x3,12);
    pmic_config_interface(0x803C,0x3,0x3,0);
    pmic_config_interface(0x804A,0x0,0x3,2);
    pmic_config_interface(0x8D1A,0xFF,0xFF,4);
    pmic_config_interface(0x8D28,0xFF,0xFF,4);
 
    #ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT
    {
        kal_uint32 is_m3_en = 0;
        pmic_read_interface(0x805E, &is_m3_en, 0x1, 2); //RGS_M3_EN
        if(is_m3_en==1)
        {
            mt6332_upmu_set_rg_pwm_en(1);
            battery_xlog_printk(BAT_LOG_CRTI,"[swchr_hw_init] mt6332_upmu_set_rg_pwm_en(1)\n"); 
        }
		else
		{
		    battery_xlog_printk(BAT_LOG_CRTI,"[swchr_hw_init] mt6332_upmu_set_rg_pwm_en(0)\n"); 
	    }
    }
    #endif
   
    //------------
    swchr_dump_register();
}

static kal_uint32 charging_hw_init(void *data)
{
    kal_uint32 status = STATUS_OK;

    swchr_hw_init();
       
    return status;
}


static kal_uint32 charging_dump_register(void *data)
{
    kal_uint32 status = STATUS_OK;

    swchr_dump_register();
          
    return status;
}    

static kal_uint32 charging_enable(void *data)
{
    kal_uint32 status = STATUS_OK;
    kal_uint32 enable = *(kal_uint32*)(data);
    kal_uint32 is_m3_en = 0;

    pmic_read_interface(0x805E, &is_m3_en, 0x1, 2); //RGS_M3_EN
    battery_xlog_printk(BAT_LOG_FULL,"[charging_enable] read RGS_M3_EN=%d\n", is_m3_en); 
    
    if(KAL_TRUE == enable)
    {
        if(is_m3_en==1)
        {
		    if(Charger_enable_Flag==1)//add by longcheer_liml_09_02
		    { 
			    mt6332_upmu_set_rg_pwm_en(1);
			    printk("[charging_enable] mt6332_upmu_set_rg_pwm_en(1)\n");
		    }else{
			    mt6332_upmu_set_rg_pwm_en(0);
			    printk("[charging_enable] mt6332_upmu_set_rg_pwm_en(0)\n");
		    } 
        }        
    }
    else
    {
        #if defined(CONFIG_USB_MTK_HDRC_HCD)
        if(mt_usb_is_device())
        #endif             
        {
            if(is_m3_en==1)
            {
                mt6332_upmu_set_rg_pwm_en(0);
                battery_xlog_printk(BAT_LOG_FULL,"[charging_enable] mt6332_upmu_set_rg_pwm_en(0)\n"); 
            }
        }
    }
    
    return status;
}


static kal_uint32 charging_set_cv_voltage(void *data)
{
    kal_uint32 status = STATUS_OK;
    kal_uint16 register_value;    
    
    register_value = charging_parameter_to_value(VBAT_CV_VTH, GETARRAYNUM(VBAT_CV_VTH) ,*(kal_uint32 *)(data));
    g_cv_reg_val = register_value;

    if(*(kal_uint32 *)(data) == BATTERY_VOLT_04_340000_V)
    {
        g_cv_reg_val=0x4;
    }
    
    set_cv_volt();
    
    return status;
}     


static kal_uint32 charging_get_current(void *data)
{
    kal_uint32 status = STATUS_OK;
    kal_uint32 array_size;
    kal_uint32 reg_value;
    
    //Get current level
    array_size = GETARRAYNUM(CS_VTH);
    pmic_read_interface(MT6332_CHR_CON10, &reg_value, 0x1F, 0); 
    
    *(kal_uint32 *)data = charging_value_to_parameter(CS_VTH,array_size,reg_value);
    
    return status;
}  


static void swchr_flow_normal(kal_uint32 chr_cur_val)
{
    set_cv_volt();
    
    //set PRECC
    mt6332_upmu_set_rg_iprecc_swen(0);
    //turn off PRECC current
    mt6332_upmu_set_rg_precc_en_swen(1);
    mt6332_upmu_set_rg_precc_en(0);
    
    //set ICH
    mt6332_upmu_set_rg_ich_sel_swen(1);
    mt6332_upmu_set_rg_ich_sel(chr_cur_val);
}


static void swchr_flow_main(kal_uint32 chr_cur_val)
{
    kal_uint32 is_pwm_en = 0;
    kal_uint32 is_m3_en = 0;
    kal_uint32 is_charge_complete = 0;
    kal_uint32 is_auto_recharge = 0;
    kal_uint32 volt_vbat = 0;
    kal_uint32 reg_val = 0;
        
    pmic_read_interface(0x805E, &is_pwm_en, 0x1, 6); //RGS_PWM_EN
    battery_xlog_printk(BAT_LOG_FULL,"[swchr_flow_main] read RGS_PWM_EN=%d\n", is_pwm_en); 

    pmic_read_interface(0x805E, &is_m3_en, 0x1, 2); //RGS_M3_EN
    battery_xlog_printk(BAT_LOG_FULL,"[swchr_flow_main] read RGS_M3_EN=%d\n", is_m3_en); 

    //Notmal mode charging, ex. CC and CV
    if(is_m3_en==1)
    {
        battery_xlog_printk(BAT_LOG_FULL, "[swchr_flow_main] m3_en==%d (0x%x)\n", is_m3_en, get_pmic_mt6332_cid());

        //Notmal mode charging
        swchr_flow_normal(chr_cur_val);
    }
    //Power path mode
    else
    {   
        battery_xlog_printk(BAT_LOG_CRTI, "[swchr_flow_main] m3_en==0\n");

        //monitor VBAT
        //if VBAT >  3.8V : is HW charge complete to turn off M3, HW will re-turn-on M3 when re-charge
        //Others          : is power path mode 
        volt_vbat = PMIC_IMM_GetOneChannelValue(ADC_BATSNS_AP,2,1);
        if(volt_vbat > 3800)
        {
            battery_xlog_printk(BAT_LOG_CRTI, "[swchr_flow_main] wait HW re-turn-on M3, vbat=%d\n", volt_vbat);
        }
        else
        {           
            //set ICH
            mt6332_upmu_set_rg_ich_sel_swen(1);
            mt6332_upmu_set_rg_ich_sel(chr_cur_val);            
            
            //set PRECC
            mt6332_upmu_set_rg_iprecc_swen(1);
            mt6332_upmu_set_rg_iprecc(0x3); //450mA

            volt_vbat = PMIC_IMM_GetOneChannelValue(ADC_BATSNS_AP,2,1);
            if(volt_vbat >= 3400)
            {
                //set CV_VTH=Vsys=3.4V
                mt6332_upmu_set_rg_cv_sel(0x18); 
                mt6332_upmu_set_rg_cv_pp_sel(0x18);
                battery_xlog_printk(BAT_LOG_CRTI, "[swchr_flow_main] set CV_VTH=3.4V\n");
                        
                //enable M3
                mt6332_upmu_set_rg_precc_m3_en(1); 

                //check M3
                pmic_read_interface(0x805E, &reg_val, 0x1, 2); //RGS_M3_EN
                if(reg_val == 1)
                {
                    battery_xlog_printk(BAT_LOG_CRTI, "[swchr_flow_main] check m3_en==1 => OK\n");
                    //Notmal mode charging
                    swchr_flow_normal(chr_cur_val);

                    //move to swchr_flow_normal
                    //turn off PRECC current
                    //mt6332_upmu_set_rg_precc_en_swen(1);
                    //mt6332_upmu_set_rg_precc_en(0);
                }
                else
                {
                    battery_xlog_printk(BAT_LOG_CRTI, "[swchr_flow_main] check m3_en==0 => FAIL\n");
                }
            }
        }
        
        battery_xlog_printk(BAT_LOG_CRTI,"[swchr_flow_main] volt_vbat=%d\n", volt_vbat); 
    }

    pmic_read_interface(0x805E, &is_charge_complete, 0x1, 10); //RGS_CHARGE_COMPLETE_DET
    battery_xlog_printk(BAT_LOG_FULL,"[swchr_flow_main] read RGS_CHARGE_COMPLETE_DET=%d\n", is_charge_complete); 

    pmic_read_interface(0x805E, &is_auto_recharge, 0x1, 11); //RGS_AUTO_RECHARGE
    battery_xlog_printk(BAT_LOG_FULL,"[swchr_flow_main] read RGS_AUTO_RECHARGE=%d\n", is_auto_recharge); 

    swchr_dump_register();
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

    //---------------------------------
    swchr_flow_main(register_value);
   
    return status;
}     


static kal_uint32 charging_set_input_current(void *data)
{
    kal_uint32 status = STATUS_OK;
    kal_uint32 set_chr_current;
    kal_uint32 array_size;
    kal_uint32 register_value;
    kal_uint32 current_value = *(kal_uint32 *)data;
   
    array_size = GETARRAYNUM(CS_VTH);
    set_chr_current = bmt_find_closest_level(CS_VTH, array_size, current_value);
    register_value = charging_parameter_to_value(CS_VTH, array_size ,set_chr_current);

    //---------------------------------
    //do at charging_set_current       

    return status;
}     


static kal_uint32 charging_get_charging_status(void *data)
{
    kal_uint32 status = STATUS_OK;
    //kal_uint32 ret_val = 0;

#if 1
    if(g_sw_rechr_flag == 1)
        *(kal_uint32 *)data = KAL_TRUE;
    else
        *(kal_uint32 *)data = KAL_FALSE;    
#else    
    pmic_read_interface(0x805E, &ret_val, 0x1, 10); //RGS_CHARGE_COMPLETE_DET
    battery_xlog_printk(BAT_LOG_FULL,"[charging_get_charging_status] read RGS_CHARGE_COMPLETE_DET=%d\n", ret_val);
        
    if(ret_val == 0x1)
        *(kal_uint32 *)data = KAL_TRUE;
    else
        *(kal_uint32 *)data = KAL_FALSE;
#endif    
    
    return status;
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

kal_uint32 pp_en_count=0; //pp:power path
kal_uint32 pp_en_count_go=120; 

static kal_uint32 charging_reset_watch_dog_timer(void *data)
{
    kal_uint32 status = STATUS_OK;

    kal_uint32 cv_val = 0;
    kal_uint32 ich_low_val = 0;
    kal_uint32 is_charge_complete = 0;
    kal_uint32 is_m3_en = 0;
    
    mt6332_upmu_set_rg_chrwdt_en(1);    
    mt6332_upmu_set_rg_chrwdt_wr(1);

    set_cv_volt();

#if 1
    check_sw_recharge();
#endif

// chr_plug_out_sw_detect    
#if 1 
    if(upmu_is_chr_det()==1) //sync USB device/otg state
    {
        pmic_read_interface(0x805E, &cv_val, 0x1, 0);
        pmic_read_interface(0x8054, &ich_low_val, 0x1, 1);
        pmic_read_interface(0x805E, &is_charge_complete, 0x1, 10);
        pmic_read_interface(0x805E, &is_m3_en, 0x1, 2);

        if(cv_val == 1)
        {
            battery_xlog_printk(BAT_LOG_FULL,"[chr_plug_out_sw_detect] in CV\n");
            #if 0
            pmic_config_interface(0x8074, 0x1, 0x1, 9);
            #else
            //if(is_charge_complete==1) {pmic_config_interface(0x8074, 0x0, 0x1, 9);}
            //else                      {pmic_config_interface(0x8074, 0x1, 0x1, 9);}

            pmic_config_interface(0x8074, 0x1, 0x1, 9); // tim test 

            #endif            
            pmic_config_interface(0x8166, 0x1, 0x1,12);
            pmic_config_interface(0x8D36, 0x3, 0x3,11); //[12:11]=0x3
        }
        else
        {
            battery_xlog_printk(BAT_LOG_FULL,"[chr_plug_out_sw_detect] not in CV\n");            
            pmic_config_interface(0x8074, 0x0, 0x1, 9);
            pmic_config_interface(0x8166, 0x1, 0x1,12);
            
            #ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT
            pmic_config_interface(0x8D36, 0x3, 0x3,11); //[12:11]=0x3
            #else
            //pmic_config_interface(0x8D36, 0x0, 0x3,11); //[12:11]=0x0
            pmic_config_interface(0x8D36, 0x3, 0x3,11); //[12:11]=0x3  // tim test

            #endif
        }
            
        #if 1            
            battery_xlog_printk(BAT_LOG_FULL,"[chr_plug_out_sw_detect] Reg[0x%x]=0x%x\n", 0x8074, upmu_get_reg_value(0x8074) );
            battery_xlog_printk(BAT_LOG_FULL,"[chr_plug_out_sw_detect] ich_low_val=%d, is_charge_complete=%d, is_m3_en=%d\n", 
                ich_low_val, is_charge_complete, is_m3_en);

            //------------------------------------------------            
            if( (ich_low_val==1) || (is_charge_complete==1) )
            {                
                
                if(is_m3_en==1)
                {
                    msleep(10);
                    set_usb_dc_in_mode(0,0);
                    set_usb_dc_in_mode(0,1);                        
                    msleep(10); 
                    set_usb_dc_in_mode(1,0);                    
                }
                else
                {
                    battery_xlog_printk(BAT_LOG_CRTI,"[chr_plug_out_sw_detect] nothing 1\n");
                }                                
            }
            else
            {
                set_usb_dc_in_mode(0,0);
                battery_xlog_printk(BAT_LOG_CRTI,"[chr_plug_out_sw_detect] nothing 2\n");
            }
#ifdef CONFIG_MTK_ENABLE_PP
            //------------------------------------------------
            if(pp_en_count==pp_en_count_go)
            {
                if(is_charge_complete==1)
                {   
                    mt6332_upmu_set_rg_ch_complete_m3_off(1);
                    msleep(10); 
                    pmic_config_interface(0x8082,0x0,0x1,10);
                }
            }
            else
            {
                pp_en_count++;
                battery_xlog_printk(BAT_LOG_CRTI,"[power_path_workaround] disable power_path_workaround (%d,%d)\n", pp_en_count, pp_en_count_go);
            }
#endif
        #endif
            
        //debug
        swchr_dump_register();
        mt_swchr_debug_msg();
    
        battery_xlog_printk(BAT_LOG_FULL,"[chr_plug_out_sw_detect] Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", 
                0x8D1E, upmu_get_reg_value(0x8D1E), 0x8D2C, upmu_get_reg_value(0x8D2C), 0x816C, upmu_get_reg_value(0x816C) );
    }
    else
    {
        pp_en_count=0;
        battery_xlog_printk(BAT_LOG_FULL,"[chr_plug_out_sw_detect] no cable\n");
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
    int is_valid_charger = 0;
    kal_uint32 val=0, val_1=0;

    pmic_config_interface(0x10A, 0x1, 0xF, 8);
    pmic_config_interface(0x10A, 0x17,0xFF,0);
    pmic_read_interface(0x108,   &val,0x1, 1);
     
    if (val == 1) // double check 
    {        
        val_1 = PMIC_IMM_GetOneChannelValue(ADC_VUSB_AP, 1, 1);         
        if(val_1 < 1000)        
        {        
            battery_xlog_printk(BAT_LOG_CRTI,"CHRDET=%d but vchr=%d \n", val, val_1);
            val = 0;
        }
    }

    *(kal_bool*)(data) = val;
    battery_xlog_printk(BAT_LOG_CRTI,"CHRDET=%d\n", val);
   
    if(val == 0)
        g_charger_type = CHARGER_UNKNOWN;

    //------------------------------
    if(val == 1)
    {
        set_cv_volt();
    
        pmic_read_interface(0x8178,&is_valid_charger,0x1,11);
        battery_xlog_printk(BAT_LOG_FULL,"[charging_get_charger_det_status] is_valid_charger = %d\n", is_valid_charger); 
        
        if(is_valid_charger == 0)
        {
            //need check
            mt_swchr_debug_msg();
        }
        #if 1
        else
        {
            mt_swchr_debug_msg();
        }
        #endif
    }
          
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

    *(CHARGER_TYPE*)(data) = hw_charging_get_charger_type();
	//dengbing add
	int ncount = 0; 
	while((*(CHARGER_TYPE*)(data) == NONSTANDARD_CHARGER) && (ncount < 5)) 
	{ 
		msleep(100); 
		*(CHARGER_TYPE*)(data) = hw_charging_get_charger_type(); 
		battery_xlog_printk(BAT_LOG_CRTI, "[charging_get_charger_type][HHL] ncount = %d \n", ncount); 
		ncount++;
	}
	//
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
    {
        g_wake_reason_is_none = 0;
        *(kal_bool*)(data) = KAL_TRUE;
    }
    else if((slp_get_wake_reason() == WR_NONE) || (slp_get_wake_reason() == WR_WAKE_SRC))
    {
        if (g_wake_reason_is_none < 20) {
            g_wake_reason_is_none++;
            *(kal_bool*)(data) = KAL_FALSE;
        }
        else
        {
            g_wake_reason_is_none = 0;
            *(kal_bool*)(data) = KAL_TRUE;
        }        
    }
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
  
    battery_xlog_printk(BAT_LOG_CRTI, "charging_set_power_off\n");
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

static void ta_set_chr_current(kal_uint32 chr_current)
{
	swchr_flow_normal(chr_current);

	battery_xlog_printk(BAT_LOG_CRTI,"[0x%x]=0x%x,[0x%x]=0x%x\n", 
		0x8068, upmu_get_reg_value(0x8068),
		0x8078, upmu_get_reg_value(0x8078)
		);
}

static kal_uint32 charging_set_ta_current_pattern(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 array_size; 
	kal_uint32 ta_on_current = CHARGE_CURRENT_500_00_MA;
    kal_uint32 ta_off_current= CHARGE_CURRENT_100_00_MA;
    kal_uint32 set_ta_on_current_reg_value=0; 
    kal_uint32 set_ta_off_current_reg_value=0;
    kal_uint32 increase = *(kal_uint32*)(data);
    
    array_size = GETARRAYNUM(CS_VTH);
	//CS_VTH is higher than 550mA
    set_ta_on_current_reg_value = charging_parameter_to_value(CS_VTH, array_size ,ta_on_current);    
	//CS_VTH is lower than 130mA    
    set_ta_off_current_reg_value = charging_parameter_to_value(CS_VTH, array_size ,ta_off_current);

    pmic_config_interface(0x8D36,0x1,0x1,11); //[11]=0x1
    battery_xlog_printk(BAT_LOG_CRTI, "[charging_set_ta_current_pattern] [0x%x]=0x%x\n", 
        0x8D36, upmu_get_reg_value(0x8D36)
        );

    if(increase == KAL_TRUE)
    {
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() start (%d)\n", set_ta_off_current_reg_value);

         ta_set_chr_current(set_ta_off_current_reg_value); 
         msleep(93);
     
         // patent start
         ta_set_chr_current(set_ta_on_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() on 1\n");
         msleep(93);
         ta_set_chr_current(set_ta_off_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() off 1\n");
         msleep(93);

         
         ta_set_chr_current(set_ta_on_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() on 2\n");
         msleep(93);
         ta_set_chr_current(set_ta_off_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() off 2\n");
         msleep(93);

     
         ta_set_chr_current(set_ta_on_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() on 3\n");
         msleep(293);
         ta_set_chr_current(set_ta_off_current_reg_value);
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() off 3\n");
         msleep(93);

         
         ta_set_chr_current(set_ta_on_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() on 4\n");
         msleep(293);
         ta_set_chr_current(set_ta_off_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() off 4\n");
         msleep(93);

         
         ta_set_chr_current(set_ta_on_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() on 5\n");
         msleep(293);
         ta_set_chr_current(set_ta_off_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() off 5\n");
         msleep(93);


         ta_set_chr_current(set_ta_on_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() on 6\n");
         msleep(493);
         ta_set_chr_current(set_ta_off_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() off 6\n");
         msleep(50);
         // patent end

         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() end \n");

         ta_set_chr_current(set_ta_on_current_reg_value); 
         msleep(200);
    }
    else    //decrease
    {
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() start\n");

         ta_set_chr_current(set_ta_off_current_reg_value); 
         msleep(93);

         // patent start    
         ta_set_chr_current(set_ta_on_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() on 1");
         msleep(293);
         ta_set_chr_current(set_ta_off_current_reg_value);
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() off 1");
         msleep(93);
         
         
         ta_set_chr_current(set_ta_on_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() on 2");
         msleep(293);
         ta_set_chr_current(set_ta_off_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() off 2");
         msleep(93);

         
         ta_set_chr_current(set_ta_on_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() on 3");
         msleep(293);
         ta_set_chr_current(set_ta_off_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() off 3");
         msleep(93);

          
         ta_set_chr_current(set_ta_on_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() on 4");
         msleep(93);
         ta_set_chr_current(set_ta_off_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() off 4");
         msleep(93);

         
         ta_set_chr_current(set_ta_on_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() on 5");
         msleep(93);
         ta_set_chr_current(set_ta_off_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() off 5");
         msleep(93);

         
         ta_set_chr_current(set_ta_on_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() on 6");
         msleep(493);        
         ta_set_chr_current(set_ta_off_current_reg_value); 
         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() off 6");
         msleep(49);
         // patent end

         battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() end \n"); 

         ta_set_chr_current(set_ta_on_current_reg_value); 
    }

    return status;
}

static kal_uint32 charging_get_error_state(void)
{
	return charging_error;
}

static kal_uint32 charging_set_error_state(void *data)
{
	kal_uint32 status = STATUS_OK;
	charging_error = *(kal_uint32*)(data);
	
	return status;
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
	,charging_set_error_state
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


