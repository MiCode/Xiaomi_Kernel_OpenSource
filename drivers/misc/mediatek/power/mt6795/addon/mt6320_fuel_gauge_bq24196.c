/*****************************************************************************
 *
 * Filename:
 * ---------
 *    mt6320_fuel_gauge.c
 *
 * Project:
 * --------
 *   Maui_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of 6320 fuel gauge
 *
 * Author:
 * -------
 * -------
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * removed!
 * removed!
 * removed!
 *
 * removed!
 * removed!
 * removed!
 *
 * removed!
 * removed!
 * removed!
 *
 * removed!
 * removed!
 * removed!
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <linux/init.h>        /* For init/exit macros */
#include <linux/module.h>      /* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include <linux/xlog.h>

#include <asm/uaccess.h>
#include <asm/div64.h>
#include <mach/mt_typedefs.h>
#include <mach/hardware.h>

#include <cust_fuel_gauge.h>

#include <mach/pmic_mt6320_sw.h>
#include <mach/upmu_common.h>
#include <mach/upmu_hw.h>

#include "bq24196.h"

#if defined(CONFIG_POWER_VERIFY)

kal_int32 gFG_columb = 0;
kal_int32 gFG_current = 0;
kal_bool gFG_Is_Charging = KAL_FALSE;
kal_int32 gFG_booting_counter_I_FLAG = 0;
int gFG_15_vlot=3700;
kal_int32 gFG_DOD0 = 0;
kal_int32 gFG_DOD1 = 0;
kal_int32 gFG_voltage = 0;

void fg_qmax_update_for_aging(void)
{
}
void FGADC_Reset_SW_Parameter(void)
{
}
kal_int32 FGADC_Get_BatteryCapacity_CoulombMothod(void)
{
    return 0;
}
kal_int32 FGADC_Get_BatteryCapacity_VoltageMothod(void)
{
    return 0;
}
int get_r_fg_value(void)
{
    return 0;
}
kal_bool get_gFG_Is_Charging(void)
{
    return 0;
}
kal_int32 fgauge_read_current(void)
{
    return 0;
}
void FGADC_thread_kthread(void)
{
}
void fgauge_initialization(void)
{
}
kal_int32 get_dynamic_period(int first_use, int first_wakeup_time, int battery_capacity_level)
{
    return first_wakeup_time;
}

#else

/*****************************************************************************
 * Logging System
****************************************************************************/
int Enable_FGADC_LOG = 2;

///////////////////////////////////////////////////////////////////////////////////////////
//// Extern Functions
///////////////////////////////////////////////////////////////////////////////////////////
#define AUXADC_BATTERY_VOLTAGE_CHANNEL     0
#define AUXADC_REF_CURRENT_CHANNEL         1
#define AUXADC_CHARGER_VOLTAGE_CHANNEL     2
#define AUXADC_TEMPERATURE_CHANNEL         3

extern int get_bat_sense_volt(int times);
extern int get_i_sense_volt(int times);
extern int get_tbat_volt(int times);
extern int g_ocv_lookup_done;
extern int g_boot_charging;
extern INT16 BattVoltToTemp(UINT32 dwVolt);
extern kal_bool upmu_is_chr_det(void);
extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);

extern int g_charger_in_flag;
extern int g_SW_CHR_OUT_EN;
extern int g_HW_Charging_Done;
extern int g_HW_stop_charging;
extern int bat_volt_check_point;
extern int gForceADCsolution;
extern kal_bool batteryBufferFirst;

///////////////////////////////////////////////////////////////////////////////////////////
//// Define
///////////////////////////////////////////////////////////////////////////////////////////
#define UNIT_FGCURRENT     (158122)     // 158.122 uA
#define UNIT_FGTIME        (16)         // 0.16s
#define UNIT_FGCHARGE      (21961412)     // 0.021961412 uAh //6320

#define MAX_V_CHARGER      4000
#define CHR_OUT_CURRENT    100

static DEFINE_MUTEX(FGADC_mutex);

///////////////////////////////////////////////////////////////////////////////////////////
// Common API
///////////////////////////////////////////////////////////////////////////////////////////
kal_int32 fgauge_read_r_bat_by_v(kal_int32 voltage);
kal_int32 fgauge_get_Q_max(kal_int16 temperature);
kal_int32 fgauge_get_Q_max_high_current(kal_int16 temperature);


/*******************************************************************************
* FUNCTION
*  fgauge_get_saddles
*
* DESCRIPTION
*  This funcion is to calculate the total saddles in the battery profile
*
* PARAMETERS
*  None
*
* RETURNS
*  Number of saddles in the battery profile
*
*******************************************************************************/
int fgauge_get_saddles(void)
{
    return sizeof(battery_profile_t2) / sizeof(BATTERY_PROFILE_STRUC);
}

int fgauge_get_saddles_r_table(void)
{
    return sizeof(r_profile_t2) / sizeof(R_PROFILE_STRUC);
}

/*******************************************************************************
* FUNCTION
*  fgauge_get_profile
*
* DESCRIPTION
*  This funcion is to get the pointer to the battery profile according to
*  specified temperature.
*
* PARAMETERS
*  temperature - temperature for battery profile. this value only could be one of
*                TEMPERATURE_T1
*                TEMPERATURE_T2
*                TEMPERATURE_T3
*                TEMPERATURE_T
*
* RETURNS
*  The pointer to the battery profile according to specified temperature.
*  If the temperature is not valid, then return NULL
*
*******************************************************************************/
BATTERY_PROFILE_STRUC_P fgauge_get_profile(kal_uint32 temperature)
{
    switch (temperature)
    {
        case TEMPERATURE_T0:
            return &battery_profile_t0[0];
            break;    
        case TEMPERATURE_T1:
            return &battery_profile_t1[0];
            break;
        case TEMPERATURE_T2:
            return &battery_profile_t2[0];
            break;
        case TEMPERATURE_T3:
            return &battery_profile_t3[0];
            break;
        case TEMPERATURE_T:
            return &battery_profile_temperature[0];
            break;
        default:
            return NULL;
            break;
    }
}

R_PROFILE_STRUC_P fgauge_get_profile_r_table(kal_uint32 temperature)
{
    switch (temperature)
    {
        case TEMPERATURE_T0:
            return &r_profile_t0[0];
            break;
        case TEMPERATURE_T1:
            return &r_profile_t1[0];
            break;
        case TEMPERATURE_T2:
            return &r_profile_t2[0];
            break;
        case TEMPERATURE_T3:
            return &r_profile_t3[0];
            break;
        case TEMPERATURE_T:
            return &r_profile_temperature[0];
            break;
        default:
            return NULL;
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
// Global Variable
///////////////////////////////////////////////////////////////////////////////////////////
kal_int8 gFG_DOD0_update = 0;
kal_int32 gFG_DOD0 = 0;
kal_int32 gFG_DOD1 = 0;
kal_int32 gFG_DOD1_return = 0;
kal_int32 gFG_columb = 0;
kal_int32 gFG_columb_HW_reg = 0;
kal_int32 gFG_voltage = 0;
kal_int32 gFG_voltage_pre = -500;
kal_int32 gFG_current = 0;
kal_int32 gFG_capacity = 0;
kal_int32 gFG_capacity_by_c = 0;
kal_int32 gFG_capacity_by_c_init = 0;
kal_int32 gFG_capacity_by_v = 0;
kal_int32 gFG_columb_init = 0;
kal_int16 gFG_temp= 100;
kal_int16 gFG_pre_temp=100;
kal_int16 gFG_T_changed=5;
kal_int32 gEstBatCapacity = 0;
kal_int32 gFG_SW_CoulombCounter = 0;
kal_bool gFG_Is_Charging = KAL_FALSE;
kal_int32 gFG_bat_temperature = 0;
kal_int32 gFG_resistance_bat = 0;
kal_int32 gFG_compensate_value = 0;
kal_int32 gFG_ori_voltage = 0;
kal_int32 gFG_booting_counter_I = 0;
kal_int32 gFG_booting_counter_I_FLAG = 0;
kal_int32 gFG_BATT_CAPACITY = 0;
int vchr_kthread_index=0;
kal_int32 gFG_voltage_init=0;
kal_int32 gFG_current_auto_detect_R_fg_total=0;
kal_int32 gFG_current_auto_detect_R_fg_count=0;
kal_int32 gFG_current_auto_detect_R_fg_result=0;
kal_int32 current_get_ori=0;
int gFG_15_vlot=3700;
kal_int32 gfg_percent_check_point=50;
kal_int32 gFG_BATT_CAPACITY_init_high_current = 3600;
kal_int32 gFG_BATT_CAPACITY_aging = 3600;
int volt_mode_update_timer=0;
int volt_mode_update_time_out=6; //1mins

#define AGING_TUNING_VALUE 103

kal_int32 chip_diff_trim_value_4_0=0;
kal_int32 chip_diff_trim_value=0; // unit = 0.1

void get_hw_chip_diff_trim_value(void)
{
    kal_int32 reg_val_1 = 0;
    kal_int32 reg_val_2 = 0;
    
    #if 1
    reg_val_1 = upmu_get_reg_value(0x01C4);
    reg_val_1 = (reg_val_1 & 0xE000) >> 13;
    
    reg_val_2 = upmu_get_reg_value(0x01C6);
    reg_val_2 = (reg_val_2 & 0x0003);

    chip_diff_trim_value_4_0 = reg_val_1 | (reg_val_2 << 3);
    
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Chip_Trim] Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, chip_diff_trim_value_4_0=%d\n", 
        0x01C4, upmu_get_reg_value(0x01C4), 0x01C6, upmu_get_reg_value(0x01C6), chip_diff_trim_value_4_0);   
    
    #else
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Chip_Trim] need check reg number\n");
    #endif

    switch(chip_diff_trim_value_4_0){       
        case 0:    
            chip_diff_trim_value = 1000; 
            printk("chip_diff_trim_value = 1000; \n");
            break;
        case 1:    chip_diff_trim_value = 1005; break;
        case 2:    chip_diff_trim_value = 1010; break;
        case 3:    chip_diff_trim_value = 1015; break;
        case 4:    chip_diff_trim_value = 1020; break;
        case 5:    chip_diff_trim_value = 1025; break;
        case 6:    chip_diff_trim_value = 1030; break;
        case 7:    chip_diff_trim_value = 1035; break;
        case 8:    chip_diff_trim_value = 1040; break;
        case 9:    chip_diff_trim_value = 1045; break;
        case 10:   chip_diff_trim_value = 1050; break;
        case 11:   chip_diff_trim_value = 1055; break;
        case 12:   chip_diff_trim_value = 1060; break;
        case 13:   chip_diff_trim_value = 1065; break;
        case 14:   chip_diff_trim_value = 1070; break;
        case 15:   chip_diff_trim_value = 1075; break;
        case 31:   chip_diff_trim_value = 995; break; 
        case 30:   chip_diff_trim_value = 990; break; 
        case 29:   chip_diff_trim_value = 985; break; 
        case 28:   chip_diff_trim_value = 980; break; 
        case 27:   chip_diff_trim_value = 975; break; 
        case 26:   chip_diff_trim_value = 970; break; 
        case 25:   chip_diff_trim_value = 965; break; 
        case 24:   chip_diff_trim_value = 960; break; 
        case 23:   chip_diff_trim_value = 955; break; 
        case 22:   chip_diff_trim_value = 950; break; 
        case 21:   chip_diff_trim_value = 945; break; 
        case 20:   chip_diff_trim_value = 940; break; 
        case 19:   chip_diff_trim_value = 935; break; 
        case 18:   chip_diff_trim_value = 930; break; 
        case 17:   chip_diff_trim_value = 925; break; 
        default:
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Chip_Trim] Invalid value(%d)\n", chip_diff_trim_value_4_0);
            break;
    }

    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Chip_Trim] %d,%d\n", 
        chip_diff_trim_value_4_0, chip_diff_trim_value);
}

kal_int32 use_chip_trim_value(kal_int32 not_trim_val)
{
    kal_int32 ret_val=0;

    ret_val=((not_trim_val*chip_diff_trim_value)/1000);

    if (Enable_FGADC_LOG == 1) {
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[use_chip_trim_value] %d -> %d\n", not_trim_val, ret_val);
    }
    
    return ret_val;
}

#if defined(CHANGE_R_FG_OFFSET)
int r_fg_offset=cust_r_fg_offset;
#else
int r_fg_offset=23;
#endif

int get_r_fg_value(void)
{
    return (R_FG_VALUE+r_fg_offset);
}

kal_bool get_gFG_Is_Charging(void)
{
    return gFG_Is_Charging;
}

void FGADC_dump_register(void)
{
    kal_uint32 reg_val = 0;
    kal_uint32 reg_num = FGADC_CON0;
    int i=0;

    for(i=reg_num ; i<=FGADC_CON19 ; i+=2)
    {
        reg_val = upmu_get_reg_value(i);
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "Reg[0x%x]=0x%x \r\n", i, reg_val);
    }
}

kal_uint32 fg_get_data_ready_status(void)
{
    kal_uint32 ret=0;
    kal_uint32 temp_val=0;

    ret=pmic_read_interface(FGADC_CON0, &temp_val, 0xFFFF, 0x0);

    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fg_get_data_ready_status] Reg[0x%x]=0x%x\r\n", FGADC_CON0, temp_val);
    }
    
    temp_val = (temp_val & (PMIC_FG_LATCHDATA_ST_MASK << PMIC_FG_LATCHDATA_ST_SHIFT)) >> PMIC_FG_LATCHDATA_ST_SHIFT;

    return temp_val;    
}

kal_uint32 fg_get_sw_clear_status(void)
{
    kal_uint32 ret=0;
    kal_uint32 temp_val=0;

    ret=pmic_read_interface(FGADC_CON0, &temp_val, 0xFFFF, 0x0);

    if (Enable_FGADC_LOG == 1) {    
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fg_get_sw_clear_status] Reg[0x%x]=0x%x\r\n", FGADC_CON0, temp_val);
    }
    
    temp_val = (temp_val & (PMIC_FG_SW_CLEAR_MASK << PMIC_FG_SW_CLEAR_SHIFT)) >> PMIC_FG_SW_CLEAR_SHIFT;

    return temp_val;    
}

///////////////////////////////////////////////////////////////////////////////////////////
//// Variables for debug UI tool
///////////////////////////////////////////////////////////////////////////////////////////
extern int bat_volt_check_point;
int g_fg_dbg_bat_volt=0;
int g_fg_dbg_bat_current=0;
int g_fg_dbg_bat_zcv=0;
int g_fg_dbg_bat_temp=0;
int g_fg_dbg_bat_r=0;
int g_fg_dbg_bat_car=0;
int g_fg_dbg_bat_qmax=0;
int g_fg_dbg_d0=0;
int g_fg_dbg_d1=0;
int g_fg_dbg_percentage=0;
int g_fg_dbg_percentage_fg=0;
int g_fg_dbg_percentage_voltmode=0;

void update_fg_dbg_tool_value(void)
{
    g_fg_dbg_bat_volt = gFG_voltage_init;

    if(gFG_Is_Charging)
        g_fg_dbg_bat_current = 1 - gFG_current - 1;
    else
        g_fg_dbg_bat_current = gFG_current;

    g_fg_dbg_bat_zcv = gFG_voltage;

    g_fg_dbg_bat_temp = gFG_temp;

    g_fg_dbg_bat_r = gFG_resistance_bat;

    g_fg_dbg_bat_car = gFG_columb;

    g_fg_dbg_bat_qmax = gFG_BATT_CAPACITY_aging;

    g_fg_dbg_d0 = gFG_DOD0;

    g_fg_dbg_d1 = gFG_DOD1;

    g_fg_dbg_percentage = bat_volt_check_point;    

    g_fg_dbg_percentage_fg = gFG_capacity_by_c;

    g_fg_dbg_percentage_voltmode = gfg_percent_check_point;
}

///////////////////////////////////////////////////////////////////////////////////////////
// SW algorithm
///////////////////////////////////////////////////////////////////////////////////////////

/*******************************************************************************
* FUNCTION
*  fgauge_read_temperature
*
* DESCRIPTION
*  This funcion is to get current battery temperature from AUXADC
*
* PARAMETERS
*  none
*
* RETURNS
*  current battery temperature, in unit of Celsius
*
* NOTE
*
*******************************************************************************/
kal_int32 fgauge_read_temperature(void)
{
    int bat_temperature_volt=0;
    int bat_temperature=0;
        
    bat_temperature_volt = get_tbat_volt(5);
    bat_temperature = BattVoltToTemp(bat_temperature_volt);
    gFG_bat_temperature = bat_temperature;

    return bat_temperature;
}

/*******************************************************************************
* FUNCTION
*  fgauge_read_columb
*
* DESCRIPTION
*  This funcion is to get the columb counter value throughout fuel gauge, in unit of mAh
*
* PARAMETERS
*  none
*
* RETURNS
*  current battery current consumption, in unit of mini-amp
*  positive value means the power that the battery comsumed.
*  negtive value means the power that is charged to the battery.
*
* NOTE
*
*******************************************************************************/
void dump_nter(void)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[dump_nter] upmu_get_fg_nter_29_16 = 0x%x\r\n", 
        upmu_get_fg_nter_29_16());
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[dump_nter] upmu_get_fg_nter_15_00 = 0x%x\r\n", 
        upmu_get_fg_nter_15_00());
}

void dump_car(void)
{    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[dump_car] upmu_get_fg_car_35_32 = 0x%x\r\n", 
        upmu_get_fg_car_35_32());
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[dump_car] upmu_get_fg_car_31_16 = 0x%x\r\n", 
        upmu_get_fg_car_31_16());
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[dump_car] upmu_get_fg_car_15_00 = 0x%x\r\n", 
        upmu_get_fg_car_15_00());
}

kal_int32 fgauge_read_columb(void)
{
    kal_uint32 uvalue32_CAR = 0;
    kal_uint32 uvalue32_CAR_MSB = 0;
    kal_int32 dvalue_CAR = 0;
    int m = 0;
    int Temp_Value = 0;
    kal_uint32 ret = 0;

// HW Init
    //(1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2
    //(2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for digital
    //(3)    i2c_write (0x61, 0x69, 0x28); // Set current mode, auto-calibration mode and 32KHz clock source
    //(4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC

//Read HW Raw Data
    //(1)    Set READ command
    ret=pmic_config_interface(FGADC_CON0, 0x0200, 0xFF00, 0x0);
    //(2)    Keep i2c read when status = 1 (0x06)
    m=0;
    while ( fg_get_data_ready_status() == 0 )
    {        
        m++;
        if(m>1000)
        {
            if (Enable_FGADC_LOG == 1){
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[fgauge_read_columb] fg_get_data_ready_status timeout 1 !\r\n");
            }
            break;
        }
    }
    //(3)    Read FG_CURRENT_OUT[28:14]
    //(4)    Read FG_CURRENT_OUT[35]
    uvalue32_CAR =  ( upmu_get_fg_car_15_00() ) >> 14;
    uvalue32_CAR |= ( (upmu_get_fg_car_31_16())&0x3FFF ) << 2;
    //uvalue32_CAR = uvalue32_CAR & 0xFFFF;
    gFG_columb_HW_reg = uvalue32_CAR;
    uvalue32_CAR_MSB = (upmu_get_fg_car_35_32() & 0x0F)>>3;
    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] fgauge_read_columb : FG_CAR = 0x%x\r\n", uvalue32_CAR);
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] fgauge_read_columb : uvalue32_CAR_MSB = 0x%x\r\n", uvalue32_CAR_MSB);
    }
    //(5)    (Read other data)
    //(6)    Clear status to 0
    ret=pmic_config_interface(FGADC_CON0, 0x0800, 0xFF00, 0x0);
    //(7)    Keep i2c read when status = 0 (0x08)
    //while ( fg_get_sw_clear_status() != 0 ) 
    m=0;
    while ( fg_get_data_ready_status() != 0 )
    {         
        m++;
        if(m>1000)
        {
            if (Enable_FGADC_LOG == 1){
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[fgauge_read_columb] fg_get_data_ready_status timeout 2 !\r\n");
            }
            break;
        }
    }    
    //(8)    Recover original settings
    ret=pmic_config_interface(FGADC_CON0, 0x0000, 0xFF00, 0x0);    

//calculate the real world data    
    dvalue_CAR = (kal_int32) uvalue32_CAR;    

    if(uvalue32_CAR == 0)
    {
        Temp_Value = 0;
    }
    else if(uvalue32_CAR == 65535) // 0xffff
    {
        Temp_Value = 0;
    }
    else if(uvalue32_CAR_MSB == 0x1)
    {
        //dis-charging
        Temp_Value = dvalue_CAR - 65535; // keep negative value        
    }
    else
    {
        //charging
        Temp_Value = (int) dvalue_CAR;
    }    
    Temp_Value = ( ((Temp_Value*35986)/10) + (5) )/10; //[28:14]'s LSB=359.86 uAh
    dvalue_CAR = Temp_Value / 1000; //mAh

    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] fgauge_read_columb : dvalue_CAR = %d\r\n", dvalue_CAR);
    }
    
    #if (OSR_SELECT_7 == 1)
        dvalue_CAR = dvalue_CAR * 8;
        if (Enable_FGADC_LOG == 1) {
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] fgauge_read_columb : dvalue_CAR update to %d\r\n", dvalue_CAR);
        }
    #endif        
    
//Auto adjust value
    if(R_FG_VALUE != 20)
    {
        if (Enable_FGADC_LOG == 1) {
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] Auto adjust value deu to the Rfg is %d\n Ori CAR=%d, ", R_FG_VALUE, dvalue_CAR);            
        }
        dvalue_CAR = (dvalue_CAR*20)/R_FG_VALUE;
        if (Enable_FGADC_LOG == 1) {
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "new CAR=%d\n", dvalue_CAR);            
        }        
    }

    dvalue_CAR = ((dvalue_CAR*CAR_TUNE_VALUE)/100);

    dvalue_CAR = use_chip_trim_value(dvalue_CAR);

    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] fgauge_read_columb : final dvalue_CAR = %d\r\n", dvalue_CAR);
    }

    if (Enable_FGADC_LOG == 1){
        dump_nter();
        dump_car();
    }    

    return dvalue_CAR;
}

kal_int32 fgauge_read_columb_reset(void)
{
    kal_uint32 uvalue32_CAR = 0;
    kal_uint32 uvalue32_CAR_MSB = 0;
    kal_int32 dvalue_CAR = 0;
    int m = 0;
    int Temp_Value = 0;
    kal_uint32 ret = 0;

// HW Init
    //(1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2
    //(2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for digital
    //(3)    i2c_write (0x61, 0x69, 0x28); // Set current mode, auto-calibration mode and 32KHz clock source
    //(4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC

//Read HW Raw Data
    //(1)    Set READ command
    ret=pmic_config_interface(FGADC_CON0, 0x7300, 0xFF00, 0x0);
    //(2)    Keep i2c read when status = 1 (0x06)
    m=0;
    while ( fg_get_data_ready_status() == 0 )
    {        
        m++;
        if(m>1000)
        {
            if (Enable_FGADC_LOG == 1){
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[fgauge_read_columb] fg_get_data_ready_status timeout 1 !\r\n");
            }
            break;
        }
    }
    //(3)    Read FG_CURRENT_OUT[28:14]
    //(4)    Read FG_CURRENT_OUT[35]
    uvalue32_CAR =  ( upmu_get_fg_car_15_00() ) >> 14;
    uvalue32_CAR |= ( (upmu_get_fg_car_31_16())&0x3FFF ) << 2;
    //uvalue32_CAR = uvalue32_CAR & 0xFFFF;
    gFG_columb_HW_reg = uvalue32_CAR;
    uvalue32_CAR_MSB = (upmu_get_fg_car_35_32() & 0x0F)>>3;
    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] fgauge_read_columb : FG_CAR = 0x%x\r\n", uvalue32_CAR);
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] fgauge_read_columb : uvalue32_CAR_MSB = 0x%x\r\n", uvalue32_CAR_MSB);
    }
    //(5)    (Read other data)
    //(6)    Clear status to 0
    ret=pmic_config_interface(FGADC_CON0, 0x0800, 0xFF00, 0x0);
    //(7)    Keep i2c read when status = 0 (0x08)
    //while ( fg_get_sw_clear_status() != 0 )
    m=0;
    while ( fg_get_data_ready_status() != 0 )
    {         
        m++;
        if(m>1000)
        {
            if (Enable_FGADC_LOG == 1){
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[fgauge_read_columb] fg_get_data_ready_status timeout 2 !\r\n");
            }
            break;
        }
    }    
    //(8)    Recover original settings
    ret=pmic_config_interface(FGADC_CON0, 0x0000, 0xFF00, 0x0);

//calculate the real world data    
    dvalue_CAR = (kal_int32) uvalue32_CAR;    

    if(uvalue32_CAR == 0)
    {
        Temp_Value = 0;
    }
    else if(uvalue32_CAR == 65535) // 0xffff
    {
        Temp_Value = 0;
    }    
    else if(uvalue32_CAR_MSB == 0x1)    
    {
        //dis-charging
        Temp_Value = dvalue_CAR - 65535; // keep negative value        
    }
    else
    {
        //charging
        Temp_Value = (int) dvalue_CAR;
    }    
    Temp_Value = ( ((Temp_Value*35986)/10) + (5) )/10; //[28:14]'s LSB=359.86 uAh
    dvalue_CAR = Temp_Value / 1000; //mAh

    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] fgauge_read_columb : dvalue_CAR = %d\r\n", dvalue_CAR);
    }
    
    #if (OSR_SELECT_7 == 1)
        dvalue_CAR = dvalue_CAR * 8;
        if (Enable_FGADC_LOG == 1) {
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] fgauge_read_columb : dvalue_CAR update to %d\r\n", dvalue_CAR);
        }
    #endif        
    
//Auto adjust value
    if(R_FG_VALUE != 20)
    {
        if (Enable_FGADC_LOG == 1) {
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] Auto adjust value deu to the Rfg is %d\n Ori CAR=%d, ", R_FG_VALUE, dvalue_CAR);            
        }
        dvalue_CAR = (dvalue_CAR*20)/R_FG_VALUE;
        if (Enable_FGADC_LOG == 1) {
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "new CAR=%d\n", dvalue_CAR);            
        }        
    }

    dvalue_CAR = ((dvalue_CAR*CAR_TUNE_VALUE)/100);

    dvalue_CAR = use_chip_trim_value(dvalue_CAR);

    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] fgauge_read_columb : final dvalue_CAR = %d\r\n", dvalue_CAR);
    }

    if (Enable_FGADC_LOG == 1){
        dump_nter();
        dump_car();
    }    

    return dvalue_CAR;
}

/*******************************************************************************
* FUNCTION
*  fgauge_read_current
*
* DESCRIPTION
*  This funcion is to get current battery current consumption throughout fuel gauge
*
* PARAMETERS
*  none
*
* RETURNS
*  current battery current consumption, in unit of mini-amp
*  positive value means battery is comsuming power.
*  negtive value means battery is under charging.
*
* NOTE
*
*******************************************************************************/
kal_int32 fgauge_read_current(void)
{
    kal_uint16 uvalue16 = 0;
    kal_int32 dvalue = 0; 
    int m = 0;
    kal_int64 Temp_Value = 0;
    kal_int32 Current_Compensate_Value=0;
    kal_uint32 ret = 0;

// HW Init
    //(1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2    
    //(2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for digital
    //(3)    i2c_write (0x61, 0x69, 0x28); // Set current mode, auto-calibration mode and 32KHz clock source
    //(4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC

//Read HW Raw Data
    //(1)    Set READ command
    ret=pmic_config_interface(FGADC_CON0, 0x0200, 0xFF00, 0x0);
    //(2)     Keep i2c read when status = 1 (0x06)
    m=0;
    while ( fg_get_data_ready_status() == 0 )
    {        
        m++;
        if(m>1000)
        {
            if (Enable_FGADC_LOG == 1){
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[fgauge_read_current] fg_get_data_ready_status timeout 1 !\r\n");
            }
            break;
        }
    }
    //(3)    Read FG_CURRENT_OUT[15:08]
    //(4)    Read FG_CURRENT_OUT[07:00]
    uvalue16 = upmu_get_fg_current_out();
    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] fgauge_read_current : FG_CURRENT = %x\r\n", uvalue16);
    }
    //(5)    (Read other data)
    //(6)    Clear status to 0
    ret=pmic_config_interface(FGADC_CON0, 0x0800, 0xFF00, 0x0);
    //(7)    Keep i2c read when status = 0 (0x08)
	//while ( fg_get_sw_clear_status() != 0 )
    m=0;
    while ( fg_get_data_ready_status() != 0 )
    {         
        m++;
        if(m>1000)
        {
            if (Enable_FGADC_LOG == 1){
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[fgauge_read_current] fg_get_data_ready_status timeout 2 !\r\n");
            }
            break;
        }
    }    
    //(8)    Recover original settings
    ret=pmic_config_interface(FGADC_CON0, 0x0000, 0xFF00, 0x0);

//calculate the real world data    
    dvalue = (kal_uint32) uvalue16;
    if( dvalue == 0 )
    {
        Temp_Value = (kal_int64) dvalue;
        gFG_Is_Charging = KAL_FALSE;
    }
    else if( dvalue > 32767 ) // > 0x8000
    {
        Temp_Value = (kal_int64)(dvalue - 65535);
        Temp_Value = Temp_Value - (Temp_Value*2);
        gFG_Is_Charging = KAL_FALSE;
    }
    else
    {
        Temp_Value = (kal_int64) dvalue;
        gFG_Is_Charging = KAL_TRUE;
    }    
    
    Temp_Value = Temp_Value * UNIT_FGCURRENT;    
    do_div(Temp_Value, 100000);
    dvalue = (kal_uint32)Temp_Value;  

    current_get_ori = dvalue;

    if (Enable_FGADC_LOG == 1) 
    {
        if( gFG_Is_Charging == KAL_TRUE )
        {
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] fgauge_read_current : current(charging) = %d mA\r\n", dvalue);
        }
        else
        {
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] fgauge_read_current : current(discharging) = %d mA\r\n", dvalue);
        }
    }

// Auto adjust value
    if(R_FG_VALUE != 20)
    {
        if (Enable_FGADC_LOG == 1) {
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] Auto adjust value deu to the Rfg is %d\n Ori current=%d, ", R_FG_VALUE, dvalue);            
        }
        dvalue = (dvalue*20)/R_FG_VALUE;
        if (Enable_FGADC_LOG == 1) {
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "new current=%d\n", dvalue);            
        }
    }

// K current 
    if(R_FG_BOARD_SLOPE != R_FG_BOARD_BASE)
    {
        dvalue = ( (dvalue*R_FG_BOARD_BASE) + (R_FG_BOARD_SLOPE/2) ) / R_FG_BOARD_SLOPE;
    }

// current compensate
    if(gFG_Is_Charging==KAL_TRUE)
    {
        dvalue = dvalue + Current_Compensate_Value;
    }
    else
    {
        dvalue = dvalue - Current_Compensate_Value;
    }

    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "ori current=%d\n", dvalue);            
    }
    
    dvalue = ((dvalue*CAR_TUNE_VALUE)/100);

    dvalue = use_chip_trim_value(dvalue);

    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "final current=%d (ratio=%d)\n", dvalue, CAR_TUNE_VALUE);            
    }

    return dvalue;
}

/*******************************************************************************
* FUNCTION
*  fgauge_read_voltage
*
* DESCRIPTION
*  This funcion is to get current battery voltage throughout AUXADC
*
* PARAMETERS
*  none
*
* RETURNS
*  current battery voltage, in unit of mini-voltage
*
* NOTE
*
*******************************************************************************/
kal_int32 fgauge_read_voltage(void)
{    
    int vol_battery;
        
//    vol_battery = get_bat_sense_volt(15);
    vol_battery = get_i_sense_volt(15);

    
    if(gFG_voltage_pre == -500)
    {
        gFG_voltage_pre = vol_battery; // for init
        
        return vol_battery;
    }

    return vol_battery;
}

/*******************************************************************************
* FUNCTION
*  fgauge_compensate_battery_voltage
*
* DESCRIPTION
*  This funcion is to compensate the actual battery voltage if the voltage is
*  measured under closed circuit.
*
* PARAMETERS
*  None
*
* RETURNS
*  The value to compensate the battery voltage.
*
*******************************************************************************/
kal_int32 fgauge_compensate_battery_voltage(kal_int32 ori_voltage)
{
    kal_int32 ret_compensate_value = 0;

    gFG_ori_voltage = ori_voltage;
    gFG_resistance_bat = fgauge_read_r_bat_by_v(ori_voltage); // Ohm
    ret_compensate_value = (gFG_current * (gFG_resistance_bat + R_FG_VALUE)) / 1000;
    ret_compensate_value = (ret_compensate_value+(10/2)) / 10;

    if (gFG_Is_Charging == KAL_TRUE) 
    {
        ret_compensate_value = ret_compensate_value - (ret_compensate_value*2);
    }

    gFG_compensate_value = ret_compensate_value;

    //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[CompensateVoltage] Ori_voltage:%d, compensate_value:%d, gFG_resistance_bat:%d, gFG_current:%d\r\n", 
    //    ori_voltage, ret_compensate_value, gFG_resistance_bat, gFG_current);

    return ret_compensate_value;
}

kal_int32 fgauge_compensate_battery_voltage_recursion(kal_int32 ori_voltage, kal_int32 recursion_time)
{
    kal_int32 ret_compensate_value = 0;
    kal_int32 temp_voltage_1 = ori_voltage;
    kal_int32 temp_voltage_2 = temp_voltage_1;
    int i = 0;

    for(i=0 ; i < recursion_time ; i++) 
    {
        gFG_resistance_bat = fgauge_read_r_bat_by_v(temp_voltage_2); // Ohm
        ret_compensate_value = (gFG_current * (gFG_resistance_bat + R_FG_VALUE)) / 1000;        
        ret_compensate_value = (ret_compensate_value+(10/2)) / 10;
        
        if (gFG_Is_Charging == KAL_TRUE) 
        {
            ret_compensate_value = ret_compensate_value - (ret_compensate_value*2);
        }
        temp_voltage_2 = temp_voltage_1 + ret_compensate_value;

        if (Enable_FGADC_LOG == 1)
        {
            xlog_printk(ANDROID_LOG_VERBOSE, "Power/Battery", "[fgauge_compensate_battery_voltage_recursion] %d,%d,%d,%d\r\n", 
                temp_voltage_1, temp_voltage_2, gFG_resistance_bat, ret_compensate_value);
        }
    }
    
    gFG_resistance_bat = fgauge_read_r_bat_by_v(temp_voltage_2); // Ohm
    ret_compensate_value = (gFG_current * (gFG_resistance_bat + R_FG_VALUE + FG_METER_RESISTANCE)) / 1000;    
    ret_compensate_value = (ret_compensate_value+(10/2)) / 10; 
    
    if (gFG_Is_Charging == KAL_TRUE) 
    {
        ret_compensate_value = ret_compensate_value - (ret_compensate_value*2);
    }

    gFG_compensate_value = ret_compensate_value;

    if (Enable_FGADC_LOG == 1)
    {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_compensate_battery_voltage_recursion] %d,%d,%d,%d\r\n", 
            temp_voltage_1, temp_voltage_2, gFG_resistance_bat, ret_compensate_value);
    }

    //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[CompensateVoltage] Ori_voltage:%d, compensate_value:%d, gFG_resistance_bat:%d, gFG_current:%d\r\n", 
    //    ori_voltage, ret_compensate_value, gFG_resistance_bat, gFG_current);

    return ret_compensate_value;
}

void fgauge_read_avg_I_V(void)
{
    int vol[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    int vol_sum = 0, vol_temp = 0;
    kal_int32 cur_sum = 0;
    int i, j;

    for(i=0;i<14;i++)
    {
        vol[i] = get_i_sense_volt(1);
        gFG_current = fgauge_read_current();

        vol[i] = vol[i] + fgauge_compensate_battery_voltage_recursion(vol[i],5); //mV
        vol[i] = vol[i] + OCV_BOARD_COMPESATE;

        cur_sum += gFG_current;
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_avg_I_V] #%d voltage[%d], current[%d]\r\n", i, vol[i], gFG_current);
    }

    //sorting vol
    for(i=0; i<14 ; i++)
    {
        for(j=i; j<14 ; j++)
        {
            if(vol[j] < vol[i])
            {
                vol_temp = vol[j];
                vol[j] = vol[i];
                vol[i] = vol_temp;
            }
        }
    }

    for(i=2;i<12;i++)
    {
        if (Enable_FGADC_LOG == 1)
        {
            //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_avg_I_V] vol[%d] = %d\r\n", i, vol[i]);
        }
        vol_sum += vol[i];
    }

    gFG_voltage = vol_sum / 10;
    gFG_current = cur_sum / 14;

//    if (Enable_FGADC_LOG == 1)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[fgauge_read_avg_I_V] AVG voltage[%d], current[%d]\r\n", gFG_voltage, gFG_current);
    }
}

/*******************************************************************************
* FUNCTION
*  fgauge_construct_battery_profile
*
* DESCRIPTION
*  This funcion is to construct battery profile according current battery temperature
*
* PARAMETERS
*  temperature [IN]- current battery temperature
*  temp_profile_p [OUT]- pointer to the battery profile data structure
*
* RETURNS
*  None
*
*******************************************************************************/
void fgauge_construct_battery_profile(kal_int32 temperature, BATTERY_PROFILE_STRUC_P temp_profile_p)
{
    BATTERY_PROFILE_STRUC_P low_profile_p, high_profile_p;
    kal_int32 low_temperature, high_temperature;
    int i, saddles;
    kal_int32 temp_v_1 = 0, temp_v_2 = 0;

    if (temperature <= TEMPERATURE_T1)
    {
        low_profile_p    = fgauge_get_profile(TEMPERATURE_T0);
        high_profile_p   = fgauge_get_profile(TEMPERATURE_T1);
        low_temperature  = (-10);
        high_temperature = TEMPERATURE_T1;
        
        if(temperature < low_temperature)
        {
            temperature = low_temperature;
        }
    }
    else if (temperature <= TEMPERATURE_T2)
    {
        low_profile_p    = fgauge_get_profile(TEMPERATURE_T1);
        high_profile_p   = fgauge_get_profile(TEMPERATURE_T2);
        low_temperature  = TEMPERATURE_T1;
        high_temperature = TEMPERATURE_T2;
        
        if(temperature < low_temperature)
        {
            temperature = low_temperature;
        }
    }
    else
    {
        low_profile_p    = fgauge_get_profile(TEMPERATURE_T2);
        high_profile_p   = fgauge_get_profile(TEMPERATURE_T3);
        low_temperature  = TEMPERATURE_T2;
        high_temperature = TEMPERATURE_T3;
        
        if(temperature > high_temperature)
        {
            temperature = high_temperature;
        }
    }

    saddles = fgauge_get_saddles();

    for (i = 0; i < saddles; i++)
    {
        if( ((high_profile_p + i)->voltage) > ((low_profile_p + i)->voltage) )
        {
            temp_v_1 = (high_profile_p + i)->voltage;
            temp_v_2 = (low_profile_p + i)->voltage;    

            (temp_profile_p + i)->voltage = temp_v_2 +
            (
                (
                    (temperature - low_temperature) * 
                    (temp_v_1 - temp_v_2)
                ) / 
                (high_temperature - low_temperature)                
            );
        }
        else
        {
            temp_v_1 = (low_profile_p + i)->voltage;
            temp_v_2 = (high_profile_p + i)->voltage;

            (temp_profile_p + i)->voltage = temp_v_2 +
            (
                (
                    (high_temperature - temperature) * 
                    (temp_v_1 - temp_v_2)
                ) / 
                (high_temperature - low_temperature)                
            );
        }
    
        (temp_profile_p + i)->percentage = (high_profile_p + i)->percentage;
#if 0        
        (temp_profile_p + i)->voltage = temp_v_2 +
            (
                (
                    (temperature - low_temperature) * 
                    (temp_v_1 - temp_v_2)
                ) / 
                (high_temperature - low_temperature)                
            );
#endif
    }

    
    // Dumpt new battery profile
    for (i = 0; i < saddles ; i++)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "<DOD,Voltage> at %d = <%d,%d>\r\n",
            temperature, (temp_profile_p+i)->percentage, (temp_profile_p+i)->voltage);
    }
    
}

void fgauge_construct_r_table_profile(kal_int32 temperature, R_PROFILE_STRUC_P temp_profile_p)
{
    R_PROFILE_STRUC_P low_profile_p, high_profile_p;
    kal_int32 low_temperature, high_temperature;
    int i, saddles;
    kal_int32 temp_v_1 = 0, temp_v_2 = 0;
    kal_int32 temp_r_1 = 0, temp_r_2 = 0;

    if (temperature <= TEMPERATURE_T1)
    {
        low_profile_p    = fgauge_get_profile_r_table(TEMPERATURE_T0);
        high_profile_p   = fgauge_get_profile_r_table(TEMPERATURE_T1);
        low_temperature  = (-10);
        high_temperature = TEMPERATURE_T1;
        
        if(temperature < low_temperature)
        {
            temperature = low_temperature;
        }
    }
    else if (temperature <= TEMPERATURE_T2)
    {
        low_profile_p    = fgauge_get_profile_r_table(TEMPERATURE_T1);
        high_profile_p   = fgauge_get_profile_r_table(TEMPERATURE_T2);
        low_temperature  = TEMPERATURE_T1;
        high_temperature = TEMPERATURE_T2;
        
        if(temperature < low_temperature)
        {
            temperature = low_temperature;
        }
    }
    else
    {
        low_profile_p    = fgauge_get_profile_r_table(TEMPERATURE_T2);
        high_profile_p   = fgauge_get_profile_r_table(TEMPERATURE_T3);
        low_temperature  = TEMPERATURE_T2;
        high_temperature = TEMPERATURE_T3;
        
        if(temperature > high_temperature)
        {
            temperature = high_temperature;
        }
    }

    saddles = fgauge_get_saddles_r_table();

    /* Interpolation for V_BAT */
    for (i = 0; i < saddles; i++)
    {
        if( ((high_profile_p + i)->voltage) > ((low_profile_p + i)->voltage) )
        {
            temp_v_1 = (high_profile_p + i)->voltage;
            temp_v_2 = (low_profile_p + i)->voltage;    

            (temp_profile_p + i)->voltage = temp_v_2 +
            (
                (
                    (temperature - low_temperature) * 
                    (temp_v_1 - temp_v_2)
                ) / 
                (high_temperature - low_temperature)                
            );
        }
        else
        {
            temp_v_1 = (low_profile_p + i)->voltage;
            temp_v_2 = (high_profile_p + i)->voltage;

            (temp_profile_p + i)->voltage = temp_v_2 +
            (
                (
                    (high_temperature - temperature) * 
                    (temp_v_1 - temp_v_2)
                ) / 
                (high_temperature - low_temperature)                
            );
        }

#if 0    
        //(temp_profile_p + i)->resistance = (high_profile_p + i)->resistance;
        
        (temp_profile_p + i)->voltage = temp_v_2 +
            (
                (
                    (temperature - low_temperature) * 
                    (temp_v_1 - temp_v_2)
                ) / 
                (high_temperature - low_temperature)                
            );
#endif
    }

    /* Interpolation for R_BAT */
    for (i = 0; i < saddles; i++)
    {
        if( ((high_profile_p + i)->resistance) > ((low_profile_p + i)->resistance) )
        {
            temp_r_1 = (high_profile_p + i)->resistance;
            temp_r_2 = (low_profile_p + i)->resistance;    

            (temp_profile_p + i)->resistance = temp_r_2 +
            (
                (
                    (temperature - low_temperature) * 
                    (temp_r_1 - temp_r_2)
                ) / 
                (high_temperature - low_temperature)                
            );
        }
        else
        {
            temp_r_1 = (low_profile_p + i)->resistance;
            temp_r_2 = (high_profile_p + i)->resistance;

            (temp_profile_p + i)->resistance = temp_r_2 +
            (
                (
                    (high_temperature - temperature) * 
                    (temp_r_1 - temp_r_2)
                ) / 
                (high_temperature - low_temperature)                
            );
        }

#if 0    
        //(temp_profile_p + i)->voltage = (high_profile_p + i)->voltage;
        
        (temp_profile_p + i)->resistance = temp_r_2 +
            (
                (
                    (temperature - low_temperature) * 
                    (temp_r_1 - temp_r_2)
                ) / 
                (high_temperature - low_temperature)                
            );
#endif
    }

    // Dumpt new r-table profile
    for (i = 0; i < saddles ; i++)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "<Rbat,VBAT> at %d = <%d,%d>\r\n",
            temperature, (temp_profile_p+i)->resistance, (temp_profile_p+i)->voltage);
    }
    
}


/*******************************************************************************
* FUNCTION
*  fgauge_get_dod0
*
* DESCRIPTION
*  This funcion is to get the DOD0 of the battery
*
* PARAMETERS
*  voltage [IN] - current battery voltage
*  temperature [IN]- current battery temperature
*  voltage [IN]- current battery voltage is OCV or not
*
* RETURNS
*  None
*
* NOTE
*  fgauge_construct_battery_profile() SHOULD be called first before use fgauge_get_dod0()
*  DOD0 is the depth of discharging, for example, if the vbat < 3.2, the DOD0 is 100
*
*******************************************************************************/
kal_int32 fgauge_get_dod0(kal_int32 voltage, kal_int32 temperature, kal_bool bOcv)
{
    kal_int32 dod0 = 0;
    int i=0, saddles=0, jj=0;
    BATTERY_PROFILE_STRUC_P profile_p;
    R_PROFILE_STRUC_P profile_p_r_table;

/* R-Table (First Time) */    
    // Re-constructure r-table profile according to current temperature
    profile_p_r_table = fgauge_get_profile_r_table(TEMPERATURE_T);
    if (profile_p_r_table == NULL)
    {
        xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[FGADC] fgauge_get_profile_r_table : create table fail !\r\n");
    }
    fgauge_construct_r_table_profile(temperature, profile_p_r_table);

    // Re-constructure battery profile according to current temperature
    profile_p = fgauge_get_profile(TEMPERATURE_T);
    if (profile_p == NULL)
    {
        xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[FGADC] fgauge_get_profile : create table fail !\r\n");
        return 100;
    }
    fgauge_construct_battery_profile(temperature, profile_p);

    // Get total saddle points from the battery profile
    saddles = fgauge_get_saddles();

    // If the input voltage is not OCV, compensate to ZCV due to battery loading
    // Compasate battery voltage from current battery voltage
    jj=0;
    if (bOcv == KAL_FALSE)
    { 
        while( gFG_current == 0 )
        {
            gFG_current = fgauge_read_current();
            if(jj > 10)
                break;
            jj++;
        }
        //voltage = voltage + fgauge_compensate_battery_voltage(voltage); //mV
        voltage = voltage + fgauge_compensate_battery_voltage_recursion(voltage,5); //mV
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC] compensate_battery_voltage, voltage=%d\r\n", voltage);
    }
    
    // If battery voltage is less then mimimum profile voltage, then return 100
    // If battery voltage is greater then maximum profile voltage, then return 0
    if (voltage > (profile_p+0)->voltage)
    {
        return 0;
    }    
    if (voltage < (profile_p+saddles-1)->voltage)
    {
        return 100;
    }

    // get DOD0 according to current temperature
    for (i = 0; i < saddles - 1; i++)
    {
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "Try <%d,%d> on %d\r\n", (profile_p+i)->voltage, (profile_p+i)->percentage, voltage);
    
        if ((voltage <= (profile_p+i)->voltage) && (voltage >= (profile_p+i+1)->voltage))
        {
            dod0 = (profile_p+i)->percentage +
                (
                    (
                        ( ((profile_p+i)->voltage) - voltage ) * 
                        ( ((profile_p+i+1)->percentage) - ((profile_p + i)->percentage) ) 
                    ) /
                    ( ((profile_p+i)->voltage) - ((profile_p+i+1)->voltage) )
                );

            //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "DOD=%d\r\n", dod0);
            
            break;
        }
    }

#if 0
    // Dumpt new battery profile
    for (i = 0; i < saddles ; i++)
    {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "<Voltage,DOD> at %d = <%d,%d>\r\n",
            gFG_bat_temperature, (profile_p+i)->voltage, (profile_p+i)->percentage);
    }
#endif

    return dod0;
}

/*******************************************************************************
* FUNCTION
*  fgauge_update_dod
*
* DESCRIPTION
*  This funcion is to update the battery DOD status according to the columb
*  counter value
*
* PARAMETERS
*  None
*
* RETURNS
*  None
*
*******************************************************************************/
extern int g_HW_Charging_Done;
void fg_qmax_update_for_aging(void)
{
    if(g_HW_Charging_Done == 1) // charging full
    {
        if(gFG_DOD0 > 85)
        {
            gFG_BATT_CAPACITY_aging = ( ( (gFG_columb*1000)+(5*gFG_DOD0) ) / gFG_DOD0 ) / 10;

            // tuning
            gFG_BATT_CAPACITY_aging = (gFG_BATT_CAPACITY_aging * 100) / AGING_TUNING_VALUE;
            
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[fg_qmax_update_for_aging] need update : gFG_columb=%d, gFG_DOD0=%d, new_qmax=%d\r\n", 
                gFG_columb, gFG_DOD0, gFG_BATT_CAPACITY_aging);
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[fg_qmax_update_for_aging] no update : gFG_columb=%d, gFG_DOD0=%d, new_qmax=%d\r\n", 
                gFG_columb, gFG_DOD0, gFG_BATT_CAPACITY_aging);
        }
    }
    else
    {
        if (Enable_FGADC_LOG == 1){
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fg_qmax_update_for_aging] g_HW_Charging_Done=%d\r\n", g_HW_Charging_Done);
        }
    }
}

int g_update_qmax_flag=1;
kal_int32 fgauge_update_dod(void)
{
    kal_int32 FG_dod_1 = 0;
    int adjust_coulomb_counter=CAR_TUNE_VALUE;

    if(gFG_DOD0 > 100)
    {
        gFG_DOD0=100;
        if (Enable_FGADC_LOG == 1){
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_update_dod] gFG_DOD0 set to 100, gFG_columb=%d\r\n", 
                gFG_columb);
        }
    }
    else if(gFG_DOD0 < 0)
    {
        gFG_DOD0=0;
        if (Enable_FGADC_LOG == 1){
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_update_dod] gFG_DOD0 set to 0, gFG_columb=%d\r\n", 
                gFG_columb);
        }
    }
    else
    {
    }    

    gFG_temp = fgauge_read_temperature();
    
    if(g_update_qmax_flag == 1)
    {
        gFG_BATT_CAPACITY = fgauge_get_Q_max(gFG_temp);
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_update_dod] gFG_BATT_CAPACITY=%d, gFG_BATT_CAPACITY_aging=%d, gFG_BATT_CAPACITY_init_high_current=%d\r\n", 
            gFG_BATT_CAPACITY, gFG_BATT_CAPACITY_aging, gFG_BATT_CAPACITY_init_high_current);        
        g_update_qmax_flag = 0;
    }
    
    FG_dod_1 =  gFG_DOD0 - ((gFG_columb*100)/gFG_BATT_CAPACITY_aging);    
    
    if (Enable_FGADC_LOG == 1){
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_update_dod] FG_dod_1=%d, adjust_coulomb_counter=%d, gFG_columb=%d, gFG_DOD0=%d, gFG_temp=%d, gFG_BATT_CAPACITY=%d\r\n", 
            FG_dod_1, adjust_coulomb_counter, gFG_columb, gFG_DOD0, gFG_temp, gFG_BATT_CAPACITY);
    }

    if(FG_dod_1 > 100)
    {
        FG_dod_1=100;
        if (Enable_FGADC_LOG == 1){    
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_update_dod] FG_dod_1 set to 100, gFG_columb=%d\r\n", 
                gFG_columb);
        }
    }
    else if(FG_dod_1 < 0)
    {
        FG_dod_1=0;
        if (Enable_FGADC_LOG == 1){
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_update_dod] FG_dod_1 set to 0, gFG_columb=%d\r\n", 
                gFG_columb);
        }
    }
    else
    {
    }

    return FG_dod_1;
}

/*******************************************************************************
* FUNCTION
*  fgauge_read_capacity
*
* DESCRIPTION
*  This funcion is to get current battery capacity
*
* PARAMETERS
*  type=0: Use voltage to calculate capacity,
*  type=1: Use DOD0 and columb counter to calculate capacity
*
* RETURNS
*  current battery capacity, in unit of percentage
*
* NOTE
*
*******************************************************************************/
kal_int32 fgauge_read_capacity(kal_int32 type)
{
    kal_int32 voltage;
    kal_int32 temperature;
    kal_int32 dvalue = 0;
    
    kal_int32 C_0mA=0;
    kal_int32 C_400mA=0;
    kal_int32 dvalue_new=0;    

    if (type == 0) // for initialization
    {
        // Use voltage to calculate capacity
        voltage = fgauge_read_voltage(); // in unit of mV
        temperature = fgauge_read_temperature();                               
        dvalue = fgauge_get_dod0(voltage, temperature, KAL_FALSE); // need compensate vbat
    }
    else
    {
        // Use DOD0 and columb counter to calculate capacity
        dvalue = fgauge_update_dod(); // DOD1 = DOD0 + (-CAR)/Qmax
    }
    //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity] %d\r\n", dvalue);

    gFG_DOD1 = dvalue;

    //User View on HT~LT----------------------------------------------------------
    gFG_temp = fgauge_read_temperature();
    C_0mA = fgauge_get_Q_max(gFG_temp);
    C_400mA = fgauge_get_Q_max_high_current(gFG_temp);
    if(C_0mA > C_400mA)
    {
        dvalue_new = (100-dvalue) - ( ( (C_0mA-C_400mA) * (dvalue) ) / C_400mA );
        dvalue = 100 - dvalue_new;
    }
    if (Enable_FGADC_LOG == 1){
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity] %d,%d,%d,%d,%d,D1=%d,D0=%d\r\n", 
            gFG_temp, C_0mA, C_400mA, dvalue, dvalue_new, gFG_DOD1, gFG_DOD0);
    }
    //----------------------------------------------------------------------------

    #if 0
    //Battery Aging update ----------------------------------------------------------
    dvalue_new = dvalue;
    dvalue = ( (dvalue_new * gFG_BATT_CAPACITY_init_high_current * 100) / gFG_BATT_CAPACITY_aging ) / 100;
    if (Enable_FGADC_LOG >= 1){
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity] dvalue=%d, dvalue_new=%d, gFG_BATT_CAPACITY_init_high_current=%d, gFG_BATT_CAPACITY_aging=%d\r\n", 
            dvalue, dvalue_new, gFG_BATT_CAPACITY_init_high_current, gFG_BATT_CAPACITY_aging);
    }
    //----------------------------------------------------------------------------
    #endif

    gFG_DOD1_return = dvalue;
    dvalue = 100 - gFG_DOD1_return;

    if(dvalue <= 1)
    {
        dvalue=1;
        if (Enable_FGADC_LOG == 1){
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity] dvalue<=1 and set dvalue=1 !!\r\n");
        }
    }

    return dvalue;
}

/*******************************************************************************
* FUNCTION
*  fgauge_read_capacity_by_v
*
* DESCRIPTION
*  This funcion is the Fuel Gauge main algorithm
*
* PARAMETERS
*  None
*
* RETURNS
*  None
*
*******************************************************************************/
kal_int32 fgauge_read_capacity_by_v(void)
{    
    int i = 0, saddles = 0;
    BATTERY_PROFILE_STRUC_P profile_p;
    kal_int32 ret_percent = 0;

    profile_p = fgauge_get_profile(TEMPERATURE_T);
    if (profile_p == NULL)
    {
        xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[FGADC] fgauge get ZCV profile : fail !\r\n");
        return 100;
    }

    saddles = fgauge_get_saddles();

    if (gFG_voltage > (profile_p+0)->voltage)
    {
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity_by_v] 100:%d,%d\r\n", gFG_voltage, (profile_p+0)->voltage);
        return 100; // battery capacity, not dod
    }    
    if (gFG_voltage < (profile_p+saddles-1)->voltage)
    {
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity_by_v] 0:%d,%d\r\n", gFG_voltage, (profile_p+saddles-1)->voltage);
        return 0; // battery capacity, not dod
    }

    for (i = 0; i < saddles - 1; i++)
    {
        if ((gFG_voltage <= (profile_p+i)->voltage) && (gFG_voltage >= (profile_p+i+1)->voltage))
        {
            ret_percent = (profile_p+i)->percentage +
                (
                    (
                        ( ((profile_p+i)->voltage) - gFG_voltage ) * 
                        ( ((profile_p+i+1)->percentage) - ((profile_p + i)->percentage) ) 
                    ) /
                    ( ((profile_p+i)->voltage) - ((profile_p+i+1)->voltage) )
                );         
            
            break;
        }
        
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity_by_v] gFG_voltage=%d\r\n", gFG_voltage);
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity_by_v] (profile_p+i)->percentag=%d\r\n", (profile_p+i)->percentage);
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity_by_v] ((profile_p+i+1)->percentage)=%d\r\n", ((profile_p+i+1)->percentage));
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity_by_v] ((profile_p+i)->voltage)=%d\r\n", ((profile_p+i)->voltage));
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity_by_v] ((profile_p+i+1)->voltage) =%d\r\n", ((profile_p+i+1)->voltage));
    }
    ret_percent = 100 - ret_percent;

    return ret_percent;
}

kal_int32 fgauge_read_r_bat_by_v(kal_int32 voltage)
{    
    int i = 0, saddles = 0;
    R_PROFILE_STRUC_P profile_p;
    kal_int32 ret_r = 0;

    profile_p = fgauge_get_profile_r_table(TEMPERATURE_T);
    if (profile_p == NULL)
    {
        xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[FGADC] fgauge get R-Table profile : fail !\r\n");
        return (profile_p+0)->resistance;
    }

    saddles = fgauge_get_saddles_r_table();

    if (voltage > (profile_p+0)->voltage)
    {
        return (profile_p+0)->resistance; 
    }    
    if (voltage < (profile_p+saddles-1)->voltage)
    {
        return (profile_p+saddles-1)->resistance; 
    }

    for (i = 0; i < saddles - 1; i++)
    {
        if ((voltage <= (profile_p+i)->voltage) && (voltage >= (profile_p+i+1)->voltage))
        {
            ret_r = (profile_p+i)->resistance +
                (
                    (
                        ( ((profile_p+i)->voltage) - voltage ) * 
                        ( ((profile_p+i+1)->resistance) - ((profile_p + i)->resistance) ) 
                    ) /
                    ( ((profile_p+i)->voltage) - ((profile_p+i+1)->voltage) )
                );
            break;
        }
    }

    return ret_r;
}

kal_int32 fgauge_read_v_by_capacity(int bat_capacity)
{    
    int i = 0, saddles = 0;
    BATTERY_PROFILE_STRUC_P profile_p;
    kal_int32 ret_volt = 0;

    profile_p = fgauge_get_profile(TEMPERATURE_T);
    if (profile_p == NULL)
    {
        xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[fgauge_read_v_by_capacity] fgauge get ZCV profile : fail !\r\n");
        return 3700;
    }

    saddles = fgauge_get_saddles();

    if (bat_capacity < (profile_p+0)->percentage)
    {        
        return 3700;         
    }    
    if (bat_capacity > (profile_p+saddles-1)->percentage)
    {        
        return 3700;
    }

    for (i = 0; i < saddles - 1; i++)
    {
        if ((bat_capacity >= (profile_p+i)->percentage) && (bat_capacity <= (profile_p+i+1)->percentage))
        {
            ret_volt = (profile_p+i)->voltage -
                (
                    (
                        ( bat_capacity - ((profile_p+i)->percentage) ) * 
                        ( ((profile_p+i)->voltage) - ((profile_p+i+1)->voltage) ) 
                    ) /
                    ( ((profile_p+i+1)->percentage) - ((profile_p+i)->percentage) )
                );         

            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_v_by_capacity] ret_volt=%d\r\n", ret_volt);
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_v_by_capacity] (profile_p+i)->percentag=%d\r\n", (profile_p+i)->percentage);
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_v_by_capacity] ((profile_p+i+1)->percentage)=%d\r\n", ((profile_p+i+1)->percentage));
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_v_by_capacity] ((profile_p+i)->voltage)=%d\r\n", ((profile_p+i)->voltage));
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_v_by_capacity] ((profile_p+i+1)->voltage) =%d\r\n", ((profile_p+i+1)->voltage));
            
            break;
        }        
    }    

    return ret_volt;
}

/*******************************************************************************
* Zero Cost FG : OAM (only auxadc mode)
*******************************************************************************/
kal_int32 oam_v_ocv_1=0;
kal_int32 oam_v_ocv_2=0;
kal_int32 oam_r_1=0;
kal_int32 oam_r_2=0;
kal_int32 oam_d0=0;
kal_int32 oam_i_ori=0;

kal_int32 oam_i_1=0;
kal_int32 oam_i_2=0;
kal_int32 oam_car_1=0;
kal_int32 oam_car_2=0;
kal_int32 oam_d_1=1;
kal_int32 oam_d_2=1;
kal_int32 oam_d_3=1;
kal_int32 oam_d_3_pre=0;
kal_int32 oam_d_4=0;
kal_int32 oam_d_4_pre=0;
kal_int32 oam_d_5=0;

int oam_init_i=0;
int oam_run_i=0;

int d5_count=0;
int d5_count_time=60;
int d5_count_time_rate=1;

extern int get_charger_type(void);

kal_int32 fgauge_read_d_by_v(kal_int32 volt_bat)
{    
    int i = 0, saddles = 0;
    BATTERY_PROFILE_STRUC_P profile_p;
    kal_int32 ret_d = 0;

    profile_p = fgauge_get_profile(TEMPERATURE_T);
    if (profile_p == NULL)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] fgauge get ZCV profile : fail !\r\n");
        return 100;
    }

    saddles = fgauge_get_saddles();

    if (volt_bat > (profile_p+0)->voltage)
    {
        return 0; 
    }    
    if (volt_bat < (profile_p+saddles-1)->voltage)
    {
        return 100; 
    }

    for (i = 0; i < saddles - 1; i++)
    {
        if ((volt_bat <= (profile_p+i)->voltage) && (volt_bat >= (profile_p+i+1)->voltage))
        {
            ret_d = (profile_p+i)->percentage +
                (
                    (
                        ( ((profile_p+i)->voltage) - volt_bat ) * 
                        ( ((profile_p+i+1)->percentage) - ((profile_p + i)->percentage) ) 
                    ) /
                    ( ((profile_p+i)->voltage) - ((profile_p+i+1)->voltage) )
                );         
            
            break;
        }
        
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity_by_v] gFG_voltage=%d\r\n", gFG_voltage);
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity_by_v] (profile_p+i)->percentag=%d\r\n", (profile_p+i)->percentage);
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity_by_v] ((profile_p+i+1)->percentage)=%d\r\n", ((profile_p+i+1)->percentage));
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity_by_v] ((profile_p+i)->voltage)=%d\r\n", ((profile_p+i)->voltage));
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_capacity_by_v] ((profile_p+i+1)->voltage) =%d\r\n", ((profile_p+i+1)->voltage));
    }

    return ret_d;
}

kal_int32 fgauge_read_v_by_d(int d_val)
{    
    int i = 0, saddles = 0;
    BATTERY_PROFILE_STRUC_P profile_p;
    kal_int32 ret_volt = 0;

    profile_p = fgauge_get_profile(TEMPERATURE_T);
    if (profile_p == NULL)
    {
        xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[fgauge_read_v_by_capacity] fgauge get ZCV profile : fail !\r\n");
        return 3700;
    }

    saddles = fgauge_get_saddles();

    if (d_val < (profile_p+0)->percentage)
    {        
        return 3700;         
    }    
    if (d_val > (profile_p+saddles-1)->percentage)
    {        
        return 3700;
    }

    for (i = 0; i < saddles - 1; i++)
    {
        if ((d_val >= (profile_p+i)->percentage) && (d_val <= (profile_p+i+1)->percentage))
        {
            ret_volt = (profile_p+i)->voltage -
                (
                    (
                        ( d_val - ((profile_p+i)->percentage) ) * 
                        ( ((profile_p+i)->voltage) - ((profile_p+i+1)->voltage) ) 
                    ) /
                    ( ((profile_p+i+1)->percentage) - ((profile_p+i)->percentage) )
                );         

            //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_v_by_capacity] ret_volt=%d\r\n", ret_volt);
            //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_v_by_capacity] (profile_p+i)->percentag=%d\r\n", (profile_p+i)->percentage);
            //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_v_by_capacity] ((profile_p+i+1)->percentage)=%d\r\n", ((profile_p+i+1)->percentage));
            //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_v_by_capacity] ((profile_p+i)->voltage)=%d\r\n", ((profile_p+i)->voltage));
            //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_read_v_by_capacity] ((profile_p+i+1)->voltage) =%d\r\n", ((profile_p+i+1)->voltage));
            
            break;
        }        
    }    

    return ret_volt;
}

kal_int32 mtk_imp_tracking(kal_int32 ori_voltage, kal_int32 ori_current, kal_int32 recursion_time)
{
    kal_int32 ret_compensate_value = 0;
    kal_int32 temp_voltage_1 = ori_voltage;
    kal_int32 temp_voltage_2 = temp_voltage_1;
    int i = 0;

    for(i=0 ; i < recursion_time ; i++) 
    {
        gFG_resistance_bat = fgauge_read_r_bat_by_v(temp_voltage_2); 
        ret_compensate_value = ( (ori_current) * (gFG_resistance_bat + R_FG_VALUE)) / 1000;
        ret_compensate_value = (ret_compensate_value+(10/2)) / 10; 
        temp_voltage_2 = temp_voltage_1 + ret_compensate_value;

        if (Enable_FGADC_LOG == 1) {
            xlog_printk(ANDROID_LOG_INFO, "Power/FGOAM", "[mtk_imp_tracking] temp_voltage_2=%d,temp_voltage_1=%d,ret_compensate_value=%d,gFG_resistance_bat=%d\n", 
                temp_voltage_2,temp_voltage_1,ret_compensate_value,gFG_resistance_bat);
        }
    }
    
    gFG_resistance_bat = fgauge_read_r_bat_by_v(temp_voltage_2); 
    ret_compensate_value = ( (ori_current) * (gFG_resistance_bat + R_FG_VALUE + FG_METER_RESISTANCE)) / 1000;    
    ret_compensate_value = (ret_compensate_value+(10/2)) / 10; 

    gFG_compensate_value = ret_compensate_value;

    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_INFO, "Power/FGOAM", "[mtk_imp_tracking] temp_voltage_2=%d,temp_voltage_1=%d,ret_compensate_value=%d,gFG_resistance_bat=%d\n", 
            temp_voltage_2,temp_voltage_1,ret_compensate_value,gFG_resistance_bat);    
    }

    return ret_compensate_value;
}

kal_int32 g_hw_ocv_tune_value = 8;

int get_hw_ocv(void)
{
    kal_int32 adc_result_reg=0;
    kal_int32 adc_result=0;
    kal_int32 r_val_temp=4;    

#if defined(SWCHR_POWER_PATH)
    adc_result_reg = upmu_get_rg_adc_out_wakeup_swchr_trim();
    adc_result = (adc_result_reg*r_val_temp*1200)/1024;
    xlog_printk(ANDROID_LOG_INFO, "Power/FGOAM", "[oam] get_hw_ocv (swchr) : adc_result_reg=%d, adc_result=%d\n", 
        adc_result_reg, adc_result);
#else
    adc_result_reg = upmu_get_rg_adc_out_wakeup_pchr_trim();
    adc_result = (adc_result_reg*r_val_temp*1200)/1024;    
    xlog_printk(ANDROID_LOG_INFO, "Power/FGOAM", "[oam] get_hw_ocv (pchr) : adc_result_reg=%d, adc_result=%d\n", 
        adc_result_reg, adc_result);
#endif

    adc_result += g_hw_ocv_tune_value;

    return adc_result;
}

void oam_init(void)
{
    oam_v_ocv_1 = get_hw_ocv();
    oam_v_ocv_2 = get_hw_ocv();
    
    oam_r_1 = gFG_resistance_bat;
    oam_r_2 = gFG_resistance_bat;
    oam_d0 = gFG_DOD0;
    oam_i_ori = gFG_current;

    if(oam_init_i == 0)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/FGOAM", "[oam_init] oam_v_ocv_1,oam_v_ocv_2,oam_r_1,oam_r_2,oam_d0,oam_i_ori\n");
        oam_init_i=1;
    }

    xlog_printk(ANDROID_LOG_INFO, "Power/FGOAM", "[oam_init] %d,%d,%d,%d,%d,%d\n", 
        oam_v_ocv_1, oam_v_ocv_2, oam_r_1, oam_r_2, oam_d0, oam_i_ori);
}

void oam_run(void)
{
    int vol_bat=0;
    int vol_bat_hw_ocv=0;
    int d_hw_ocv=0;
    
//    vol_bat = get_bat_sense_volt(15);
    vol_bat = get_i_sense_volt(15);


    vol_bat_hw_ocv = get_hw_ocv();
    d_hw_ocv = fgauge_read_d_by_v(vol_bat_hw_ocv);

    oam_i_1 = (((oam_v_ocv_1-vol_bat)*1000)*10) / oam_r_1;    //0.1mA
    oam_i_2 = (((oam_v_ocv_2-vol_bat)*1000)*10) / oam_r_2;    //0.1mA

    oam_car_1 = (oam_i_1*10/3600) + oam_car_1; //0.1mAh
    oam_car_2 = (oam_i_2*10/3600) + oam_car_2; //0.1mAh

    oam_d_1 = oam_d0 + (oam_car_1*100/10)/gFG_BATT_CAPACITY_aging;
    if(oam_d_1 < 0)   oam_d_1 = 0;
    if(oam_d_1 > 100) oam_d_1 = 100;
    
    oam_d_2 = oam_d0 + (oam_car_2*100/10)/gFG_BATT_CAPACITY_aging;
    if(oam_d_2 < 0)   oam_d_2 = 0;
    if(oam_d_2 > 100) oam_d_2 = 100;
    
    oam_v_ocv_1 = vol_bat + mtk_imp_tracking(vol_bat, oam_i_2, 5);
    
    oam_d_3 = fgauge_read_d_by_v(oam_v_ocv_1);        
    if(oam_d_3 < 0)   oam_d_3 = 0;
    if(oam_d_3 > 100) oam_d_3 = 100;

    oam_r_1 = fgauge_read_r_bat_by_v(oam_v_ocv_1);

    oam_v_ocv_2 = fgauge_read_v_by_d(oam_d_2);
    oam_r_2 = fgauge_read_r_bat_by_v(oam_v_ocv_2);    

    oam_d_4 = (oam_d_2+oam_d_3)/2;

    if(oam_d_5==0)
    {
        oam_d_5 = oam_d_3;
        oam_d_3_pre = oam_d_3;
        oam_d_4_pre = oam_d_4;
    }

    if( gFG_Is_Charging == KAL_FALSE )
    {
        d5_count_time = 60;         
    }
    else
    {
        switch(get_charger_type()){               
            case 1:    d5_count_time_rate = (((gFG_BATT_CAPACITY_aging*60*60/100/(450-50))*10)+5)/10;
                       break; 
            case 2:    d5_count_time_rate = (((gFG_BATT_CAPACITY_aging*60*60/100/(650-50))*10)+5)/10;
                       break; 
            case 3:    d5_count_time_rate = (((gFG_BATT_CAPACITY_aging*60*60/100/(450-50))*10)+5)/10;
                       break; 
            case 4:    d5_count_time_rate = (((gFG_BATT_CAPACITY_aging*60*60/100/(650-50))*10)+5)/10;
                       break;                       
            default:
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[get_charger_type] Invalid value\n");
                break;
        }

        if(d5_count_time_rate < 1)
            d5_count_time_rate = 1;

        d5_count_time = d5_count_time_rate;
    }

    if(d5_count >= d5_count_time)
    {
        if( gFG_Is_Charging == KAL_FALSE )
        {
            if( oam_d_3 > oam_d_5 )
            {
                oam_d_5 = oam_d_5 + 1;
            }
            else
            {                
                if(oam_d_4 > oam_d_5)
                {
                    oam_d_5 = oam_d_5 + 1;
                }
            }
        }
        else
        {            
            if( oam_d_5 > oam_d_3 )
            {
                oam_d_5 = oam_d_5 - 1;
            }
            else
            {                
                if(oam_d_4 < oam_d_5)
                {
                    oam_d_5 = oam_d_5 - 1;
                }
            }
        }
        d5_count = 0;
        oam_d_3_pre = oam_d_3;
        oam_d_4_pre = oam_d_4;
    }
    else
    {
        d5_count = d5_count + 10;
    }
    
    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_INFO, "Power/FGOAM", "[oam_run] %d,%d,%d,%d,%d,%d,%d\n", 
            d5_count, d5_count_time, oam_d_3_pre, oam_d_3, oam_d_4_pre, oam_d_4, oam_d_5);    
    }

    if(oam_run_i == 0)
    {
        if (Enable_FGADC_LOG == 1) {
            xlog_printk(ANDROID_LOG_INFO, "Power/FGOAM", "[oam_run] oam_i_1,oam_i_2,oam_car_1,oam_car_2,oam_d_1,oam_d_2,oam_v_ocv_1,oam_d_3,oam_r_1,oam_v_ocv_2,oam_r_2,vol_bat,vol_bat_hw_ocv,d_hw_ocv\n");
        }
        oam_run_i=1;
    }    

    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_INFO, "Power/FGOAM", "[oam_run] %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", 
            oam_i_1,oam_i_2,oam_car_1,oam_car_2,oam_d_1,oam_d_2,oam_v_ocv_1,oam_d_3,oam_r_1,oam_v_ocv_2,oam_r_2,vol_bat,vol_bat_hw_ocv,d_hw_ocv);    
    }

    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_INFO, "Power/FGOAM", "[oam_total] %d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            gFG_capacity_by_c, gFG_capacity_by_v, gfg_percent_check_point,
            oam_d_1, oam_d_2, oam_d_3, oam_d_4, oam_d_5, gFG_capacity_by_c_init, d_hw_ocv);
    }

    if (Enable_FGADC_LOG == 2) {
        xlog_printk(ANDROID_LOG_INFO, "Power/FGOAM", "[oam_total_s] %d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            gFG_capacity_by_c,        // 1
            gFG_capacity_by_v,        // 2
            gfg_percent_check_point,  // 3
            (100-oam_d_1),            // 4
            (100-oam_d_2),            // 5
            (100-oam_d_3),            // 6
            (100-oam_d_4),            // 9
            (100-oam_d_5),            // 10
            gFG_capacity_by_c_init,   // 7
            (100-d_hw_ocv)            // 8
            );          
    }

    if (Enable_FGADC_LOG == 1) {
        xlog_printk(ANDROID_LOG_INFO, "Power/FGOAM", "[oam_total_s_err] %d,%d,%d,%d,%d,%d,%d\n",
            (gFG_capacity_by_c - gFG_capacity_by_v), 
            (gFG_capacity_by_c - gfg_percent_check_point),
            (gFG_capacity_by_c - (100-oam_d_1)), 
            (gFG_capacity_by_c - (100-oam_d_2)), 
            (gFG_capacity_by_c - (100-oam_d_3)), 
            (gFG_capacity_by_c - (100-oam_d_4)), 
            (gFG_capacity_by_c - (100-oam_d_5))
            );
    }
}

/*******************************************************************************
* FUNCTION
*  fgauge_Normal_Mode_Work
*
* DESCRIPTION
*  This funcion is to simulate the Fuel Gauge algorithm
*
* PARAMETERS
*  None
*
* RETURNS
*  None
*
*******************************************************************************/
#define FG_VBAT_AVERAGE_SIZE 36 // 36*5s=180s=3mins
#define MinErrorOffset 1000 //1000mV
kal_bool gFGvbatBufferFirst = KAL_FALSE;
static unsigned short FGvbatVoltageBuffer[FG_VBAT_AVERAGE_SIZE];
static int FGbatteryIndex = 0;
static int FGbatteryVoltageSum = 0;
kal_int32 gFG_voltage_AVG = 0;
kal_int32 gFG_vbat_offset=0;
kal_int32 gFG_voltageVBAT=0;

#if defined(CHANGE_TRACKING_POINT)
int g_tracking_point = CUST_TRACKING_POINT;
#else
int g_tracking_point = 14;
#endif

kal_int32 g_rtc_fg_soc = 0;
extern int get_rtc_spare_fg_value(void);

void fgauge_Normal_Mode_Work(void)
{
    int i=0;
   
//1. Get Raw Data  
    gFG_current = fgauge_read_current();

//    gFG_voltage = fgauge_read_voltage();
    gFG_voltage_init = gFG_voltage;
//    gFG_voltage = gFG_voltage + fgauge_compensate_battery_voltage_recursion(gFG_voltage,5); //mV  
//    gFG_voltage = gFG_voltage + OCV_BOARD_COMPESATE;
    fgauge_read_avg_I_V();

    gFG_current = fgauge_read_current();
    gFG_columb = fgauge_read_columb();        

//1.1 Average FG_voltage
    /**************** Averaging : START ****************/
    if(gFG_booting_counter_I_FLAG != 0)
    {
        if (!gFGvbatBufferFirst)
        {                        
            for (i=0; i<FG_VBAT_AVERAGE_SIZE; i++) {
                FGvbatVoltageBuffer[i] = gFG_voltage;            
            }

            FGbatteryVoltageSum = gFG_voltage * FG_VBAT_AVERAGE_SIZE;

            gFG_voltage_AVG = gFG_voltage;

            gFGvbatBufferFirst = KAL_TRUE;
        }

        if(gFG_voltage >= gFG_voltage_AVG)
        {
            gFG_vbat_offset = (gFG_voltage - gFG_voltage_AVG);
        }
        else
        {
            gFG_vbat_offset = (gFG_voltage_AVG - gFG_voltage);
        }

        if(gFG_vbat_offset <= MinErrorOffset)
        {
            FGbatteryVoltageSum -= FGvbatVoltageBuffer[FGbatteryIndex];
            FGbatteryVoltageSum += gFG_voltage;
            FGvbatVoltageBuffer[FGbatteryIndex] = gFG_voltage;
            
            gFG_voltage_AVG = FGbatteryVoltageSum / FG_VBAT_AVERAGE_SIZE;
            gFG_voltage = gFG_voltage_AVG;

            FGbatteryIndex++;
            if (FGbatteryIndex >= FG_VBAT_AVERAGE_SIZE)
                FGbatteryIndex = 0;

            if (Enable_FGADC_LOG == 1)
            {
                xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FG_BUFFER] ");
                for (i=0; i<FG_VBAT_AVERAGE_SIZE; i++) {
                    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "%d,", FGvbatVoltageBuffer[i]);            
                }
                xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "\r\n");
            }
        }
        else
        {
            if (Enable_FGADC_LOG == 1){
                xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FG] Over MinErrorOffset:V=%d,Avg_V=%d, ", gFG_voltage, gFG_voltage_AVG);
            }
            
            gFG_voltage = gFG_voltage_AVG;
            
            if (Enable_FGADC_LOG == 1){
                xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "Avg_V need write back to V : V=%d,Avg_V=%d.\r\n", gFG_voltage, gFG_voltage_AVG);
            }
        }
    }
    /**************** Averaging : END ****************/
    gFG_voltageVBAT = gFG_voltage;

//2. Calculate battery capacity by VBAT    
    gFG_capacity_by_v = fgauge_read_capacity_by_v();

//3. Calculate battery capacity by Coulomb Counter
    gFG_capacity_by_c = fgauge_read_capacity(1);
    gEstBatCapacity = gFG_capacity_by_c;    

//4. update DOD0 after booting Xs
    if(gFG_booting_counter_I_FLAG == 1)
    {
        gFG_booting_counter_I_FLAG = 2;

        if(g_ocv_lookup_done == 0)
        {      
#if 0        
            //use get_hw_ocv-----------------------------------------------------------------
            gFG_voltage = get_hw_ocv();        
            gFG_capacity_by_v = fgauge_read_capacity_by_v();
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] get_hw_ocv=%d, SOC=%d\n", 
            gFG_voltage, gFG_capacity_by_v);
            //-------------------------------------------------------------------------------
#endif			
            g_rtc_fg_soc = get_rtc_spare_fg_value();
			if(g_rtc_fg_soc >= gFG_capacity_by_v)
			{
				if( ( g_rtc_fg_soc != 0 					) &&					
					( (g_rtc_fg_soc-gFG_capacity_by_v) < 20 ) &&
					( gFG_capacity_by_v > 5 )							  
					)
				{
					gFG_capacity_by_v = g_rtc_fg_soc;			 
				}
			
				xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] [1] g_rtc_fg_soc=%d, gFG_capacity_by_v=%d\n", 
					g_rtc_fg_soc, gFG_capacity_by_v);
			}
			else
			{
				if( ( g_rtc_fg_soc != 0 					) &&					
					( (gFG_capacity_by_v-g_rtc_fg_soc) < 20 ) &&
					( gFG_capacity_by_v > 5 )							  
					)
				{
					gFG_capacity_by_v = g_rtc_fg_soc;			 
				}
			
				xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] [2] g_rtc_fg_soc=%d, gFG_capacity_by_v=%d\n", 
					g_rtc_fg_soc, gFG_capacity_by_v);
			}		
            //-------------------------------------------------------------------------------
        
            gFG_capacity = gFG_capacity_by_v;
        
            gFG_capacity_by_c_init = gFG_capacity;
            gFG_capacity_by_c = gFG_capacity;
            gFG_pre_temp = gFG_temp;
        
            gFG_DOD0 = 100 - gFG_capacity;
            gFG_DOD1=gFG_DOD0;
            g_ocv_lookup_done = 1;
        }
        g_boot_charging = 0;
        bat_volt_check_point = gFG_capacity;
        gfg_percent_check_point = bat_volt_check_point;        

        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] update DOD0 after booting %d s\r\n", (MAX_BOOTING_TIME_FGCURRENT));

        //OAM (only auxadc mode)        
        oam_init();    

        #if defined(CHANGE_TRACKING_POINT)
        gFG_15_vlot = fgauge_read_v_by_capacity( (100-g_tracking_point) );
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] gFG_15_vlot = %dmV\r\n", gFG_15_vlot);        
        #else
        //gFG_15_vlot = fgauge_read_v_by_capacity(86); //14%
        gFG_15_vlot = fgauge_read_v_by_capacity( (100-g_tracking_point) );
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] gFG_15_vlot = %dmV\r\n", gFG_15_vlot);        
        if( (gFG_15_vlot > 3800) || (gFG_15_vlot < 3600) ) 
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] gFG_15_vlot(%d) over range, reset to 3700\r\n", gFG_15_vlot);
            gFG_15_vlot = 3700;
        }
        #endif        

        //double check
        if(gFG_current_auto_detect_R_fg_total == 0)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "gFG_current_auto_detect_R_fg_total=0, need double check [2]\n");
            
            gFG_current_auto_detect_R_fg_count = 0;
            
            for(i=0;i<10;i++)
            {
                gFG_current_auto_detect_R_fg_total+= fgauge_read_current();
                gFG_current_auto_detect_R_fg_count++;            
            }
        }

        gFG_current_auto_detect_R_fg_result = gFG_current_auto_detect_R_fg_total / gFG_current_auto_detect_R_fg_count;
        if(gFG_current_auto_detect_R_fg_result <= CURRENT_DETECT_R_FG)
        {
            gForceADCsolution=1;            
            
            batteryBufferFirst = KAL_FALSE; // for init array values when measuring by AUXADC 
            
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] Detect NO Rfg, use AUXADC report. (%d=%d/%d)(%d)\r\n", 
                gFG_current_auto_detect_R_fg_result, gFG_current_auto_detect_R_fg_total,
                gFG_current_auto_detect_R_fg_count, gForceADCsolution);            
        }
        else
        {
            if(gForceADCsolution == 0)
            {
                gForceADCsolution=0;
        
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] Detect Rfg, use FG report. (%d=%d/%d)(%d)\r\n", 
                gFG_current_auto_detect_R_fg_result, gFG_current_auto_detect_R_fg_total,
                gFG_current_auto_detect_R_fg_count, gForceADCsolution);
        }
            else
            {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] Detect Rfg, but use AUXADC report. due to gForceADCsolution=%d \r\n", 
                    gForceADCsolution);
            }
        }
    }

#ifdef CONFIG_MTK_BQ27541_SUPPORT
    gForceADCsolution=0; //always has external gauge info
#endif

}

kal_int32 fgauge_get_Q_max(kal_int16 temperature)
{
    kal_int32 ret_Q_max=0;
    kal_int32 low_temperature = 0, high_temperature = 0;
    kal_int32 low_Q_max = 0, high_Q_max = 0;

    if (temperature <= TEMPERATURE_T1)
    {
        low_temperature = (-10);
        low_Q_max = Q_MAX_NEG_10;
        high_temperature = TEMPERATURE_T1;
        high_Q_max = Q_MAX_POS_0;
        
        if(temperature < low_temperature)
        {
            temperature = low_temperature;
        }
    }
    else if (temperature <= TEMPERATURE_T2)
    {
        low_temperature = TEMPERATURE_T1;
        low_Q_max = Q_MAX_POS_0;
        high_temperature = TEMPERATURE_T2;
        high_Q_max = Q_MAX_POS_25;
        
        if(temperature < low_temperature)
        {
            temperature = low_temperature;
        }
    }
    else
    {
        low_temperature  = TEMPERATURE_T2;
        low_Q_max = Q_MAX_POS_25;
        high_temperature = TEMPERATURE_T3;
        high_Q_max = Q_MAX_POS_50;
        
        if(temperature > high_temperature)
        {
            temperature = high_temperature;
        }
    }

    ret_Q_max = low_Q_max +
    (
        (
            (temperature - low_temperature) * 
            (high_Q_max - low_Q_max)
        ) / 
        (high_temperature - low_temperature)                
    );

    if (Enable_FGADC_LOG == 1){
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_get_Q_max] Q_max = %d\r\n", ret_Q_max);
    }

    return ret_Q_max;
}

kal_int32 fgauge_get_Q_max_high_current(kal_int16 temperature)
{
    kal_int32 ret_Q_max=0;
    kal_int32 low_temperature = 0, high_temperature = 0;
    kal_int32 low_Q_max = 0, high_Q_max = 0;

    if (temperature <= TEMPERATURE_T1)
    {
        low_temperature = (-10);
        low_Q_max = Q_MAX_NEG_10_H_CURRENT;
        high_temperature = TEMPERATURE_T1;
        high_Q_max = Q_MAX_POS_0_H_CURRENT;
        
        if(temperature < low_temperature)
        {
            temperature = low_temperature;
        }
    }
    else if (temperature <= TEMPERATURE_T2)
    {
        low_temperature = TEMPERATURE_T1;
        low_Q_max = Q_MAX_POS_0_H_CURRENT;
        high_temperature = TEMPERATURE_T2;
        high_Q_max = Q_MAX_POS_25_H_CURRENT;
        
        if(temperature < low_temperature)
        {
            temperature = low_temperature;
        }
    }
    else
    {
        low_temperature  = TEMPERATURE_T2;
        low_Q_max = Q_MAX_POS_25_H_CURRENT;
        high_temperature = TEMPERATURE_T3;
        high_Q_max = Q_MAX_POS_50_H_CURRENT;
        
        if(temperature > high_temperature)
        {
            temperature = high_temperature;
        }
    }

    ret_Q_max = low_Q_max +
    (
        (
            (temperature - low_temperature) * 
            (high_Q_max - low_Q_max)
        ) / 
        (high_temperature - low_temperature)                
    );

    if (Enable_FGADC_LOG == 1){
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[fgauge_get_Q_max_high_current] Q_max = %d\r\n", ret_Q_max);
    }

    return ret_Q_max;
}


/*******************************************************************************
* FUNCTION
*  fgauge_initialization
*
* DESCRIPTION
*  This funcion is the Fuel Gauge initial function
*
* PARAMETERS
*  None
*
* RETURNS
*  None
*
*******************************************************************************/
void fgauge_initialization(void)
{    
    int i = 0;
    kal_uint32 ret=0;

    get_hw_chip_diff_trim_value();

    gFG_BATT_CAPACITY_init_high_current = Q_MAX_POS_25_H_CURRENT;    
    gFG_BATT_CAPACITY_aging = Q_MAX_POS_25;

// 1. HW initialization
//FGADC clock is 32768Hz from RTC
    //Enable FGADC in current mode at 32768Hz with auto-calibration
    #if 0    
    //write @ RG_VA2_EN (bank0, 0x0C8[0]) = 0x1    
    //write @ RG_FGADC_CK_PDN (bank1, 0x015[4]) = 0x0
    //write @ FG_VMODE (bank1, 0x069[1]) = 0x0
    //write @ FG_CLKSRC (bank1, 0x069[7]) = 0x0
    //write @ FG_CAL (bank1, 0x069 [3:2]) = 0x2
    //write @ FG_ON (bank1, 0x069 [0]) = 0x1
    #endif
    //(1)    Enable VA2
    //(2)    Enable FGADC clock for digital
    upmu_set_rg_fgadc_ana_ck_pdn(0);
    upmu_set_rg_fgadc_ck_pdn(0);
    //(3)    Set current mode, auto-calibration mode and 32KHz clock source
    ret=pmic_config_interface(FGADC_CON0, 0x0028, 0x00FF, 0x0);
    //(4)    Enable FGADC
    ret=pmic_config_interface(FGADC_CON0, 0x0029, 0x00FF, 0x0);

    //reset HW FG
    ret=pmic_config_interface(FGADC_CON0, 0x7100, 0xFF00, 0x0);
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "******** [fgauge_initialization] reset HW FG!\n" );
    
// 2. SW algorithm initialization
    gFG_voltage = fgauge_read_voltage();            
//    gFG_voltage = get_hw_ocv();

    gFG_current = fgauge_read_current();   
    i=0;
    while( gFG_current == 0 )
    {
        gFG_current = fgauge_read_current();
        if(i > 10)
            break;
        i++;
    }

    gFG_columb = fgauge_read_columb();
    gFG_temp = fgauge_read_temperature();
    if(g_ocv_lookup_done == 0)
    {
        gFG_capacity = fgauge_read_capacity(0);
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "g_ocv_loopup_done is 0, do fgauge_read_capacity(0)\n" );
    }
    else
    {
    	  fgauge_read_capacity(0); //only for parameter initialization
    }	
	
    gFG_columb_init = gFG_columb;
    gFG_capacity_by_c_init = gFG_capacity;
    gFG_capacity_by_c = gFG_capacity;
    gFG_capacity_by_v = gFG_capacity;
    gFG_pre_temp = gFG_temp;

    gFG_DOD0 = 100 - gFG_capacity;

    gFG_BATT_CAPACITY = fgauge_get_Q_max(gFG_temp);

    //FGADC_dump_register();        
    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "******** [fgauge_initialization] Done!\n" );

}

///////////////////////////////////////////////////////////////////////////////////////////
//// External API 
///////////////////////////////////////////////////////////////////////////////////////////
kal_int32 FGADC_Get_BatteryCapacity_CoulombMothod(void)
{
    return gFG_capacity_by_c;
}

kal_int32 FGADC_Get_BatteryCapacity_VoltageMothod(void)
{
    return gfg_percent_check_point;
    //return gFG_capacity_by_v;
}

kal_int32 FGADC_Get_FG_Voltage(void)
{
    return gFG_voltageVBAT;
}

extern int g_Calibration_FG;

void FGADC_Reset_SW_Parameter(void)
{    
    //volatile kal_uint16 Temp_Reg = 0;
    volatile kal_uint16 val_car = 1;
    kal_uint32 ret = 0;

#if 0
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] FGADC_Reset_SW_Parameter : Todo \r\n");
#else
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] FGADC_Reset_SW_Parameter : Start \r\n");
    gFG_SW_CoulombCounter = 0;    
    while(val_car != 0x0)
    {        
        ret=pmic_config_interface(FGADC_CON0, 0x7100, 0xFF00, 0x0);        
        gFG_columb = fgauge_read_columb_reset();
        val_car = gFG_columb;
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "#");
    }
    gFG_columb = 0;
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] FGADC_Reset_SW_Parameter : Done \r\n");

    if(g_Calibration_FG==1)
    {                        
        gFG_DOD0 = 0;
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] FG Calibration DOD0=%d and DOD1=%d \r\n", gFG_DOD0, gFG_DOD1);
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] Update DOD0(%d) by %d \r\n", gFG_DOD0, gFG_DOD1);
        gFG_DOD0 = gFG_DOD1;        
    }
#endif

}

kal_int32 g_car_instant=0;
kal_int32 g_current_instant=0;
kal_int32 g_car_sleep=0;
kal_int32 g_car_wakeup=0;
kal_int32 g_last_time=0;

kal_int32 get_dynamic_period(int first_use, int first_wakeup_time, int battery_capacity_level)
{
    kal_int32 ret_val=-1;    

#if 1
    int check_fglog=0;
    kal_int32 I_sleep=0;
    kal_int32 new_time=0;

    if(gForceADCsolution==1)
    {
        return first_wakeup_time;
    }
    
    check_fglog=Enable_FGADC_LOG;
    if(check_fglog==0)
    {
        //Enable_FGADC_LOG=1;
    }
    g_current_instant = fgauge_read_current();
    g_car_instant = fgauge_read_columb();
    if(check_fglog==0)
    {
        Enable_FGADC_LOG=0;
    }
    if(g_car_instant < 0)
    {
        g_car_instant = g_car_instant - (g_car_instant*2);
    }
    
    if(first_use == 1)
    {
        //ret_val = 30*60; /* 30 mins */
        ret_val = first_wakeup_time; 
        g_last_time = ret_val;        
        g_car_sleep = g_car_instant;
    }
    else
    {
        g_car_wakeup = g_car_instant;

        if(g_last_time==0)
            g_last_time=1;    

        if(g_car_sleep > g_car_wakeup)
        {
            g_car_sleep = g_car_wakeup;
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[get_dynamic_period] reset g_car_sleep\r\n");
        }

        I_sleep = ((g_car_wakeup-g_car_sleep)*3600)/g_last_time; // unit: second

        if(I_sleep==0)
        {
            if(check_fglog==0)
            {
                //Enable_FGADC_LOG=1;
            }
            I_sleep = fgauge_read_current();
            I_sleep = I_sleep / 10;
            if(check_fglog==0)
            {
                Enable_FGADC_LOG=0;
            }
        }
        
        if(I_sleep == 0)
        {
            new_time = first_wakeup_time;
        }
        else
        {
            new_time = ((gFG_BATT_CAPACITY*battery_capacity_level*3600)/100)/I_sleep;
        }        
        ret_val = new_time;

        if(ret_val == 0)
            ret_val = first_wakeup_time;

        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[get_dynamic_period] g_car_instant=%d, g_car_wakeup=%d, g_car_sleep=%d, I_sleep=%d, gFG_BATT_CAPACITY=%d, g_last_time=%d, new_time=%d\r\n", 
            g_car_instant, g_car_wakeup, g_car_sleep, I_sleep, gFG_BATT_CAPACITY, g_last_time, new_time);        
        
        //update parameter
        g_car_sleep = g_car_wakeup;
        g_last_time = ret_val;
    }    
#else
    ret_val = first_wakeup_time;
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[get_dynamic_period] ret_val = %d\n", first_wakeup_time);
#endif

    return ret_val;
}

///////////////////////////////////////////////////////////////////////////////////////////
//// Internal API 
///////////////////////////////////////////////////////////////////////////////////////////
void fg_voltage_mode(void)
{
    if( upmu_is_chr_det()==KAL_TRUE )
    {
        /* SOC only UP when charging */
        if ( gFG_capacity_by_v > gfg_percent_check_point ) {                        
            gfg_percent_check_point++;
        }
    }
    else
    {
        /* SOC only Done when dis-charging */
        if ( gFG_capacity_by_v < gfg_percent_check_point ) {            
            gfg_percent_check_point--;
        }
    }
        
    if (Enable_FGADC_LOG == 1) 
    {    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[FGADC_VoltageMothod] gFG_capacity_by_v=%d,gfg_percent_check_point=%d\r\n", 
            gFG_capacity_by_v, gfg_percent_check_point);
    }

}

void FGADC_thread_kthread(void)
{    
    int i=0;

    mutex_lock(&FGADC_mutex);                

    fgauge_Normal_Mode_Work();        

    if(volt_mode_update_timer >= volt_mode_update_time_out)
    {
        volt_mode_update_timer=0;

        fg_voltage_mode();
    }
    else
    {
        volt_mode_update_timer++;
    }    

    //if (Enable_FGADC_LOG >= 1) 
    //{    
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
            gFG_Is_Charging,gFG_current,
            gFG_SW_CoulombCounter,gFG_columb,gFG_voltage,gFG_capacity_by_v,gFG_capacity_by_c,gFG_capacity_by_c_init, 
            gFG_BATT_CAPACITY,gFG_BATT_CAPACITY_aging,gFG_compensate_value,gFG_ori_voltage,OCV_BOARD_COMPESATE,R_FG_BOARD_SLOPE,
            gFG_voltage_init,MinErrorOffset,gFG_DOD0,gFG_DOD1,current_get_ori,
            CAR_TUNE_VALUE,AGING_TUNING_VALUE);            
    //}
    update_fg_dbg_tool_value();

    if(gFG_booting_counter_I >= MAX_BOOTING_TIME_FGCURRENT)
    {
        gFG_booting_counter_I = 0;
        gFG_booting_counter_I_FLAG = 1;

        //double check
        if(gFG_current_auto_detect_R_fg_total == 0)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "gFG_current_auto_detect_R_fg_total=0, need double check\n");
            
            gFG_current_auto_detect_R_fg_count = 0;
            
            for(i=0;i<10;i++)
            {
                gFG_current_auto_detect_R_fg_total+= fgauge_read_current();
                gFG_current_auto_detect_R_fg_count++;            
            }
        }
    }
    else 
    {
        if(gFG_booting_counter_I_FLAG == 0)
        {
            gFG_booting_counter_I+=10;
            for(i=0;i<10;i++)
            {
                gFG_current_auto_detect_R_fg_total+= fgauge_read_current();
                gFG_current_auto_detect_R_fg_count++;            
            }
            Enable_FGADC_LOG = 0;
        }
    }

    //OAM (only auxadc mode)    
    if(oam_init_i >= 1)        
        oam_run();

    mutex_unlock(&FGADC_mutex);        
}

///////////////////////////////////////////////////////////////////////////////////////////
//// Logging System
///////////////////////////////////////////////////////////////////////////////////////////
static struct proc_dir_entry *proc_entry_fgadc;
static char proc_fgadc_data[32];  

ssize_t fgadc_log_write( struct file *filp, const char __user *buff,
                        unsigned long len, void *data )
{
    if (copy_from_user( &proc_fgadc_data, buff, len )) {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "fgadc_log_write error.\n");
        return -EFAULT;
    }

    if (proc_fgadc_data[0] == '1') {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "enable FGADC driver log system\n");
        Enable_FGADC_LOG = 1;
    } else if (proc_fgadc_data[0] == '2') {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "enable FGADC driver log system:2\n");
        Enable_FGADC_LOG = 2;    
    } else {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "Disable FGADC driver log system\n");
        Enable_FGADC_LOG = 0;
    }
    
    return len;
}

int init_proc_log_fg(void)
{
    int ret=0;
    proc_entry_fgadc = create_proc_entry( "fgadc_log", 0644, NULL );
    
    if (proc_entry_fgadc == NULL) {
        ret = -ENOMEM;
          xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "init_proc_log_fg: Couldn't create proc entry\n");
    } else {
        proc_entry_fgadc->write_proc = fgadc_log_write;
        //proc_entry->owner = THIS_MODULE;
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "init_proc_log_fg loaded.\n");
    }
  
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For Power Consumption Profile : FG_Current
///////////////////////////////////////////////////////////////////////////////////////////
kal_int32 gFG_current_inout_battery = 0;
static ssize_t show_FG_Current(struct device *dev,struct device_attribute *attr, char *buf)
{
    // Power Consumption Profile---------------------
    gFG_current = fgauge_read_current();
    if(gFG_Is_Charging==KAL_TRUE)
    {
        gFG_current_inout_battery = 0 - gFG_current;
    }
    else
    {
        gFG_current_inout_battery = gFG_current;
    }
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FG] %d\r\n", gFG_current_inout_battery);
    //-----------------------------------------------
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FG] gFG_current_inout_battery : %d\n", gFG_current_inout_battery);
    return sprintf(buf, "%d\n", gFG_current_inout_battery);
}
static ssize_t store_FG_Current(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
static DEVICE_ATTR(FG_Current, 0664, show_FG_Current, store_FG_Current);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For FG UI DEBUG
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_FG_g_fg_dbg_bat_volt(struct device *dev,struct device_attribute *attr, char *buf)
{    
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FG] g_fg_dbg_bat_volt : %d\n", g_fg_dbg_bat_volt);
    return sprintf(buf, "%d\n", g_fg_dbg_bat_volt);
}
static ssize_t store_FG_g_fg_dbg_bat_volt(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
static DEVICE_ATTR(FG_g_fg_dbg_bat_volt, 0664, show_FG_g_fg_dbg_bat_volt, store_FG_g_fg_dbg_bat_volt);
//-------------------------------------------------------------------------------------------
static ssize_t show_FG_g_fg_dbg_bat_current(struct device *dev,struct device_attribute *attr, char *buf)
{    
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FG] g_fg_dbg_bat_current : %d\n", g_fg_dbg_bat_current);
    return sprintf(buf, "%d\n", g_fg_dbg_bat_current);
}
static ssize_t store_FG_g_fg_dbg_bat_current(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
static DEVICE_ATTR(FG_g_fg_dbg_bat_current, 0664, show_FG_g_fg_dbg_bat_current, store_FG_g_fg_dbg_bat_current);
//-------------------------------------------------------------------------------------------
static ssize_t show_FG_g_fg_dbg_bat_zcv(struct device *dev,struct device_attribute *attr, char *buf)
{    
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FG] g_fg_dbg_bat_zcv : %d\n", g_fg_dbg_bat_zcv);
    return sprintf(buf, "%d\n", g_fg_dbg_bat_zcv);
}
static ssize_t store_FG_g_fg_dbg_bat_zcv(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
static DEVICE_ATTR(FG_g_fg_dbg_bat_zcv, 0664, show_FG_g_fg_dbg_bat_zcv, store_FG_g_fg_dbg_bat_zcv);
//-------------------------------------------------------------------------------------------
static ssize_t show_FG_g_fg_dbg_bat_temp(struct device *dev,struct device_attribute *attr, char *buf)
{    
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FG] g_fg_dbg_bat_temp : %d\n", g_fg_dbg_bat_temp);
    return sprintf(buf, "%d\n", g_fg_dbg_bat_temp);
}
static ssize_t store_FG_g_fg_dbg_bat_temp(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
static DEVICE_ATTR(FG_g_fg_dbg_bat_temp, 0664, show_FG_g_fg_dbg_bat_temp, store_FG_g_fg_dbg_bat_temp);
//-------------------------------------------------------------------------------------------
static ssize_t show_FG_g_fg_dbg_bat_r(struct device *dev,struct device_attribute *attr, char *buf)
{    
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FG] g_fg_dbg_bat_r : %d\n", g_fg_dbg_bat_r);
    return sprintf(buf, "%d\n", g_fg_dbg_bat_r);
}
static ssize_t store_FG_g_fg_dbg_bat_r(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
static DEVICE_ATTR(FG_g_fg_dbg_bat_r, 0664, show_FG_g_fg_dbg_bat_r, store_FG_g_fg_dbg_bat_r);
//-------------------------------------------------------------------------------------------
static ssize_t show_FG_g_fg_dbg_bat_car(struct device *dev,struct device_attribute *attr, char *buf)
{    
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FG] g_fg_dbg_bat_car : %d\n", g_fg_dbg_bat_car);
    return sprintf(buf, "%d\n", g_fg_dbg_bat_car);
}
static ssize_t store_FG_g_fg_dbg_bat_car(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
static DEVICE_ATTR(FG_g_fg_dbg_bat_car, 0664, show_FG_g_fg_dbg_bat_car, store_FG_g_fg_dbg_bat_car);
//-------------------------------------------------------------------------------------------
static ssize_t show_FG_g_fg_dbg_bat_qmax(struct device *dev,struct device_attribute *attr, char *buf)
{    
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FG] g_fg_dbg_bat_qmax : %d\n", g_fg_dbg_bat_qmax);
    return sprintf(buf, "%d\n", g_fg_dbg_bat_qmax);
}
static ssize_t store_FG_g_fg_dbg_bat_qmax(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
static DEVICE_ATTR(FG_g_fg_dbg_bat_qmax, 0664, show_FG_g_fg_dbg_bat_qmax, store_FG_g_fg_dbg_bat_qmax);
//-------------------------------------------------------------------------------------------
static ssize_t show_FG_g_fg_dbg_d0(struct device *dev,struct device_attribute *attr, char *buf)
{    
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FG] g_fg_dbg_d0 : %d\n", g_fg_dbg_d0);
    return sprintf(buf, "%d\n", g_fg_dbg_d0);
}
static ssize_t store_FG_g_fg_dbg_d0(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
static DEVICE_ATTR(FG_g_fg_dbg_d0, 0664, show_FG_g_fg_dbg_d0, store_FG_g_fg_dbg_d0);
//-------------------------------------------------------------------------------------------
static ssize_t show_FG_g_fg_dbg_d1(struct device *dev,struct device_attribute *attr, char *buf)
{    
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FG] g_fg_dbg_d1 : %d\n", g_fg_dbg_d1);
    return sprintf(buf, "%d\n", g_fg_dbg_d1);
}
static ssize_t store_FG_g_fg_dbg_d1(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
static DEVICE_ATTR(FG_g_fg_dbg_d1, 0664, show_FG_g_fg_dbg_d1, store_FG_g_fg_dbg_d1);
//-------------------------------------------------------------------------------------------
static ssize_t show_FG_g_fg_dbg_percentage(struct device *dev,struct device_attribute *attr, char *buf)
{    
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FG] g_fg_dbg_percentage : %d\n", g_fg_dbg_percentage);
    return sprintf(buf, "%d\n", g_fg_dbg_percentage);
}
static ssize_t store_FG_g_fg_dbg_percentage(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
static DEVICE_ATTR(FG_g_fg_dbg_percentage, 0664, show_FG_g_fg_dbg_percentage, store_FG_g_fg_dbg_percentage);
//-------------------------------------------------------------------------------------------
static ssize_t show_FG_g_fg_dbg_percentage_fg(struct device *dev,struct device_attribute *attr, char *buf)
{    
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FG] g_fg_dbg_percentage_fg : %d\n", g_fg_dbg_percentage_fg);
    return sprintf(buf, "%d\n", g_fg_dbg_percentage_fg);
}
static ssize_t store_FG_g_fg_dbg_percentage_fg(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
static DEVICE_ATTR(FG_g_fg_dbg_percentage_fg, 0664, show_FG_g_fg_dbg_percentage_fg, store_FG_g_fg_dbg_percentage_fg);
//-------------------------------------------------------------------------------------------
static ssize_t show_FG_g_fg_dbg_percentage_voltmode(struct device *dev,struct device_attribute *attr, char *buf)
{    
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FG] g_fg_dbg_percentage_voltmode : %d\n", g_fg_dbg_percentage_voltmode);
    return sprintf(buf, "%d\n", g_fg_dbg_percentage_voltmode);
}
static ssize_t store_FG_g_fg_dbg_percentage_voltmode(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
static DEVICE_ATTR(FG_g_fg_dbg_percentage_voltmode, 0664, show_FG_g_fg_dbg_percentage_voltmode, store_FG_g_fg_dbg_percentage_voltmode);

///////////////////////////////////////////////////////////////////////////////////////////
//// platform_driver API 
///////////////////////////////////////////////////////////////////////////////////////////
static int mt6320_fgadc_probe(struct platform_device *dev)    
{
    int ret_device_file = 0;

#if defined(CONFIG_POWER_EXT)
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] INIT : EVB \n");
#else
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] MT6320 FGADC driver probe!! \n" );

    /* FG driver init */
    //fgauge_initialization();

    /*LOG System Set*/
    init_proc_log_fg();
#endif

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Current);

    //Create File For FG UI DEBUG
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_volt);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_current);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_zcv);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_temp);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_r);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_car);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_qmax);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_d0);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_d1);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_percentage);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_percentage_fg);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_percentage_voltmode);

    return 0;
}

static int mt6320_fgadc_remove(struct platform_device *dev)    
{
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] MT6320 FGADC driver remove!! \n" );

    return 0;
}

static void mt6320_fgadc_shutdown(struct platform_device *dev)    
{
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[FGADC] MT6320 FGADC driver shutdown!! \n" );        
}

static int mt6320_fgadc_suspend(struct platform_device *dev, pm_message_t state)    
{

#if defined(CONFIG_POWER_EXT)
    xlog_printk(ANDROID_LOG_VERBOSE, "Power/Battery", "[FGADC_suspend] EVB !!\n");
#else
    xlog_printk(ANDROID_LOG_VERBOSE, "Power/Battery", "[FGADC_suspend] TODO !!\n");
#endif

    return 0;
}

static int mt6320_fgadc_resume(struct platform_device *dev)
{
#if defined(CONFIG_POWER_EXT)
    xlog_printk(ANDROID_LOG_VERBOSE, "Power/Battery", "[FGADC_RESUME] EVB !!\n");
#else
    xlog_printk(ANDROID_LOG_VERBOSE, "Power/Battery", "[FGADC_RESUME] !!\n");
#endif

    return 0;
}

struct platform_device MT6320_fgadc_device = {
        .name                = "mt6320-fgadc",
        .id                  = -1,
};

static struct platform_driver mt6320_fgadc_driver = {
    .probe        = mt6320_fgadc_probe,
    .remove       = mt6320_fgadc_remove,
    .shutdown     = mt6320_fgadc_shutdown,
    //#ifdef CONFIG_PM
    .suspend      = mt6320_fgadc_suspend,
    .resume       = mt6320_fgadc_resume,
    //#endif
    .driver       = {
        .name = "mt6320-fgadc",
    },
};

static int __init mt6320_fgadc_init(void)
{
    int ret;
    
    ret = platform_device_register(&MT6320_fgadc_device);
    if (ret) {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "****[mt6320_fgadc_driver] Unable to device register(%d)\n", ret);
        return ret;
    }
    
    ret = platform_driver_register(&mt6320_fgadc_driver);
    if (ret) {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "****[mt6320_fgadc_driver] Unable to register driver (%d)\n", ret);
        return ret;
    }

    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "****[mt6320_fgadc_driver] Initialization : DONE \n");

    return 0;

}

static void __exit mt6320_fgadc_exit (void)
{
}

module_init(mt6320_fgadc_init);
module_exit(mt6320_fgadc_exit);

MODULE_AUTHOR("James Lo");
MODULE_DESCRIPTION("MT6320 FGADC Device Driver");
MODULE_LICENSE("GPL");
#endif
