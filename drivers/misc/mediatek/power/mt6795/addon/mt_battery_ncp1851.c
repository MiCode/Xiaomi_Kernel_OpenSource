/*****************************************************************************
 *
 * Filename:
 * ---------
 *    mt_battery_ncp1851.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of mt6320 Battery charging algorithm 
 *   and the Anroid Battery service for updating the battery status
 *
 * Author:
 * -------
 * YT Lee
 *
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/xlog.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <mach/hardware.h>
#include <mach/system.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpt.h>
#include <mach/mt_boot.h>
#include <mach/mt_gpio.h>

#include <cust_battery.h>
#include "mt6320_battery.h"

#include <mach/pmic_mt6320_sw.h>
#include <mach/upmu_common.h>
#include <mach/upmu_hw.h>

#include "ncp1851.h"

extern kal_int32 gFG_capacity;
extern kal_bool gFG_Is_Charging;
int g_ocv_lookup_done = 0;
int g_boot_charging = 0;
int g_pmic_init_for_ncp1851 = 0;
extern void fgauge_precharge_init(void);
extern void fgauge_precharge_uninit(void);
extern kal_int32 fgauge_precharge_compensated_voltage(kal_int32 recursion_time);

int Enable_BATDRV_LOG = 0;
//int Enable_BATDRV_LOG = 1;

///////////////////////////////////////////////////////////////////////////////////////////
//// Thermal related flags
///////////////////////////////////////////////////////////////////////////////////////////
int g_battery_thermal_throttling_flag=0; // 0:nothing, 1:enable batTT&chrTimer, 2:disable batTT&chrTimer, 3:enable batTT, disable chrTimer
int battery_cmd_thermal_test_mode=0;
int battery_cmd_thermal_test_mode_value=0;
int g_battery_tt_check_flag=0; // 0:default enable check batteryTT, 1:default disable check batteryTT

////////////////////////////////////////////////////////////////////////////////
// JEITA 
////////////////////////////////////////////////////////////////////////////////
#if defined(MTK_JEITA_STANDARD_SUPPORT)  
int g_jeita_recharging_voltage=4110;
kal_uint32 gFGsyncTimer_jeita=0;
kal_uint32 g_default_sync_time_out_jeita=CUST_SOC_JEITA_SYNC_TIME; 
int g_temp_status=TEMP_POS_10_TO_POS_45;
kal_bool temp_error_recovery_chr_flag=KAL_TRUE;
int mtk_jeita_support_flag=1;

int g_last_temp_status=TEMP_POS_10_TO_POS_45;
int battery_temprange_change_flag=0;
#else
int mtk_jeita_support_flag=0;
#endif


///////////////////////////////////////////////////////////////////////////////////////////
//// PMIC AUXADC Related APIs
///////////////////////////////////////////////////////////////////////////////////////////
#define AUXADC_BATTERY_VOLTAGE_CHANNEL  0
#define AUXADC_REF_CURRENT_CHANNEL         1
#define AUXADC_CHARGER_VOLTAGE_CHANNEL  2
#define AUXADC_TEMPERATURE_CHANNEL         3
int g_R_BAT_SENSE = R_BAT_SENSE;
int g_R_I_SENSE = R_I_SENSE;
int g_R_CHARGER_1 = R_CHARGER_1;
int g_R_CHARGER_2 = R_CHARGER_2;

#if defined(CONFIG_POWER_VERIFY)
kal_bool upmu_is_chr_det(void)
{
    return 1;
}
EXPORT_SYMBOL(upmu_is_chr_det);

#else
///////////////////////////////////////////////////////////////////////////////////////////
//// PMIC FGADC Related APIs
///////////////////////////////////////////////////////////////////////////////////////////
extern void mt_power_off(void);
//void mt_power_off(void)
//{
//    printk("mt_power_off is empty\n");
//}
extern kal_int32 FGADC_Get_BatteryCapacity_CoulombMothod(void);
extern kal_int32 FGADC_Get_BatteryCapacity_VoltageMothod(void);
extern void FGADC_Reset_SW_Parameter(void);
extern void FGADC_thread_kthread(void);
extern void fgauge_initialization(void);
extern kal_int32 fgauge_read_capacity_by_v(void);
extern kal_int32 fgauge_read_current(void);
extern int get_r_fg_value(void);
extern kal_bool get_gFG_Is_Charging(void);

extern bool mt_usb_is_device(void);
extern void mt_usb_connect(void);
extern void mt_usb_disconnect(void);

extern CHARGER_TYPE mt_charger_type_detection(void);
extern int PMIC_IMM_GetOneChannelValue(int dwChannel, int deCount, int trimd);
extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);
extern void upmu_set_reg_value(kal_uint32 reg, kal_uint32 reg_val);

extern kal_int32 gFG_current;
extern kal_int32 gFG_voltage;
extern kal_int32 gFG_DOD0;
extern kal_int32 gFG_DOD1;
extern kal_int32 gFG_columb;
extern kal_bool gFG_Is_Charging;
extern int gFG_15_vlot;

extern BOOTMODE g_boot_mode;

extern int g_first_check;

#ifdef CONFIG_USB_MTK_HDRC_HCD
void tbl_charger_otg_vbus(int mode)
{
    if(mode&0xFF)
    {
        ncp1851_set_otg_en(0x1);
    }
    else
    {
        ncp1851_set_otg_en(0x0);
    }
};
#endif

///////////////////////////////////////////////////////////////////////////////////////////
//// PMIC HW Related APIs
///////////////////////////////////////////////////////////////////////////////////////////
void charger_hv_init(void)
{
    upmu_set_rg_vcdt_hv_vth(0xD);    //VCDT_HV_VTH, 8.5V
}

U32 get_charger_hv_status(void)
{
    return upmu_get_rgs_vcdt_hv_det();
}

void kick_charger_wdt(void)
{
    upmu_set_rg_chrwdt_td(0x0);           // CHRWDT_TD, 4s
    upmu_set_rg_chrwdt_int_en(1);         // CHRWDT_INT_EN
    upmu_set_rg_chrwdt_en(1);             // CHRWDT_EN
    upmu_set_rg_chrwdt_wr(1);             // CHRWDT_WR
}

int get_bat_sense_volt(int times)
{
    kal_uint32 ret;
	//RG_SOURCE_CH0_NORM_SEL set to 0 for bat_sense
    ret=pmic_config_interface(AUXADC_CON14, 0x0, PMIC_RG_SOURCE_CH0_NORM_SEL_MASK, PMIC_RG_SOURCE_CH0_NORM_SEL_SHIFT);	
    return PMIC_IMM_GetOneChannelValue(0,times,1);
}

int get_i_sense_volt(int times)
{
    kal_uint32 ret;
	//RG_SOURCE_CH0_NORM_SEL set to 1 for i_sense
    ret=pmic_config_interface(AUXADC_CON14, 0x1, PMIC_RG_SOURCE_CH0_NORM_SEL_MASK, PMIC_RG_SOURCE_CH0_NORM_SEL_SHIFT);	
    return PMIC_IMM_GetOneChannelValue(0,times,1); //i sense use channel 0 as well
}

int get_charger_volt(int times)
{
    return PMIC_IMM_GetOneChannelValue(2,times,1);
}

int get_tbat_volt(int times)
{
    if(upmu_get_cid() == 0x1020)
    {
        return PMIC_IMM_GetOneChannelValue(4,times,1);
    }
    else    
    {
        #if defined(ENABLE_TBAT_TREF_SUPPORT)
            if (Enable_BATDRV_LOG == 1) {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[get_tbat_volt] return PMIC_IMM_GetOneChannelValue(3,times,1);\n");
            }
            return PMIC_IMM_GetOneChannelValue(3,times,1);
        #else
            if (Enable_BATDRV_LOG == 1) {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[get_tbat_volt] return PMIC_IMM_GetOneChannelValue(4,times,1);\n");
            }
            return PMIC_IMM_GetOneChannelValue(4,times,1);
        #endif
    }
}

int get_charger_detect_status(void)
{
    return upmu_get_rgs_chrdet();
}

int PMIC_IMM_GetOneChannelValueSleep(int dwChannel, int deCount)
{
    kal_int32 adc_result = 0;

    adc_result = PMIC_IMM_GetOneChannelValue(dwChannel,deCount,1);

    return adc_result;
    
}

///////////////////////////////////////////////////////////////////////////////////////////
//// PMIC PCHR Related APIs
///////////////////////////////////////////////////////////////////////////////////////////
kal_bool upmu_is_chr_det(void)
{
    kal_uint8 bst_int;
    kal_uint32 tmp32;
    tmp32=get_charger_detect_status();
    if(tmp32 == 0)
    {
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[upmu_is_chr_det] No charger\n");
        return KAL_FALSE;
    }
    else
    {
        //xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[upmu_is_chr_det] Charger exist\n");
        if( mt_usb_is_device() )
        {
            if (Enable_BATDRV_LOG == 1) {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[upmu_is_chr_det] Charger exist and USB is not host\n");
            }
            return KAL_TRUE;
        }
        else
        {
            if (Enable_BATDRV_LOG == 1) {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[upmu_is_chr_det] Charger exist but USB is host\n");
            }
            //Monitor NCP1851 boost mode status
            if(ncp1851_get_otg_en()== 0) //boost mode disabled
            {
                if (Enable_BATDRV_LOG == 1) {
                  xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[upmu_is_chr_det] USB host mode but boost 5V not enabled\n");				  
                  if(ncp1851_get_faultint() == 1)
                  {
                      xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[upmu_is_chr_det] ncp1851 fault int detected!\n");				  				  
                      ncp1851_read_register(0x6);
                      bst_int = ncp1851_reg[0x6];
                      ncp1851_reg[0x6] = 0;
                      if(bst_int & (CON6_VBUSILIM_MASK << CON6_VBUSILIM_SHIFT))
                          xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[upmu_is_chr_det] ncp1851 boost mode fail due to vbus over current!\n"); 			  
                      if(bst_int & (CON6_VBUSOV_MASK << CON6_VBUSOV_SHIFT))
                          xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[upmu_is_chr_det] ncp1851 boost mode fail due to vbus over voltage!\n"); 			  
                      if(bst_int & (CON6_VBATLO_MASK << CON6_VBATLO_SHIFT))
                          xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[upmu_is_chr_det] ncp1851 boost mode fail due to vbat low voltage!\n"); 			  					  
                  }				  
                }		
                //ncp1851_set_otg_en(0x1); //TODO: decide if need auto retry
            }			
            return KAL_FALSE;
        }
        
        //return KAL_TRUE;
    }
}
EXPORT_SYMBOL(upmu_is_chr_det);
#endif // CONFIG_POWER_VERIFY

///////////////////////////////////////////////////////////////////////////////////////////
//// Smart Battery Structure
///////////////////////////////////////////////////////////////////////////////////////////
#define UINT32 unsigned long
#define UINT16 unsigned short
#define UINT8 unsigned char

typedef struct
{
    kal_bool   	bat_exist;
    kal_bool   	bat_full;
    kal_bool   	bat_low;
    INT32       bat_charging_state;
    INT32       bat_vol;            
    kal_bool 	charger_exist;
    INT32       pre_charging_current;
    INT32       charging_current;
    INT32       charger_vol;        
    INT32       charger_protect_status; 
    INT32       ISENSE;                
    INT32       ICharging;
    INT32       temperature;
    UINT32      total_charging_time;
    UINT32      PRE_charging_time;
    UINT32      CC_charging_time;
    UINT32      TOPOFF_charging_time;
    UINT32      POSTFULL_charging_time;
    INT32       charger_type;
    INT32       PWR_SRC;
    INT32       SOC;
    INT32       ADC_BAT_SENSE;
    INT32       ADC_I_SENSE;
} PMU_ChargerStruct;

typedef enum
{
    PMU_STATUS_OK = 0,
    PMU_STATUS_FAIL = 1,
}PMU_STATUS;

///////////////////////////////////////////////////////////////////////////////////////////
//// Global Variable
///////////////////////////////////////////////////////////////////////////////////////////
static CHARGER_TYPE CHR_Type_num = CHARGER_UNKNOWN;
static unsigned short batteryVoltageBuffer[BATTERY_AVERAGE_SIZE];
static unsigned short batteryCurrentBuffer[BATTERY_AVERAGE_SIZE];
static unsigned short batterySOCBuffer[BATTERY_AVERAGE_SIZE];
static int batteryTempBuffer[BATTERY_AVERAGE_SIZE];
static int batteryIndex = 0;
static int batteryVoltageSum = 0;
static int batteryCurrentSum = 0;
static int batterySOCSum = 0;
static int batteryTempSum = 0;
PMU_ChargerStruct BMT_status;
kal_bool g_bat_full_user_view = KAL_FALSE;
kal_bool g_Battery_Fail = KAL_FALSE;
kal_bool batteryBufferFirst = KAL_FALSE;

struct wake_lock battery_suspend_lock;

int V_PRE2CC_THRES = 3400;
int V_CC2TOPOFF_THRES = 4050;

int g_HW_Charging_Done = 0;
int g_Charging_Over_Time = 0;

int gForceADCsolution=0;

int gSyncPercentage=0;

unsigned int g_BatteryNotifyCode=0x0000;
unsigned int g_BN_TestMode=0x0000;

kal_uint32 gFGsyncTimer=0;
kal_uint32 DEFAULT_SYNC_TIME_OUT=60; //1mins

int g_Calibration_FG=0;

int g_XGPT_restart_flag=0;

#define CHR_OUT_CURRENT	50

int gSW_CV_prepare_flag=0;

int getVoltFlag = 0;
int g_bat_temperature_pre=22;

int gADC_BAT_SENSE_temp=0;
int gADC_I_SENSE_temp=0;
int gADC_I_SENSE_offset=0;

int g_battery_flag_resume=0;

int gBAT_counter_15=1;

int gFG_can_reset_flag = 1;

extern kal_int32 gFG_booting_counter_I_FLAG;

int read_tbat_value(void)
{
    return BMT_status.temperature;
}

////////////////////////////////////////////////////////////////////////////////
// EM
////////////////////////////////////////////////////////////////////////////////
int g_BAT_TemperatureR = 0;
int g_TempBattVoltage = 0;
int g_InstatVolt = 0;
int g_BatteryAverageCurrent = 0;
int g_BAT_BatterySenseVoltage = 0;
int g_BAT_ISenseVoltage = 0;
int g_BAT_ChargerVoltage = 0;

////////////////////////////////////////////////////////////////////////////////
// Definition For GPT
////////////////////////////////////////////////////////////////////////////////
static int bat_thread_timeout = 0;

static DEFINE_MUTEX(bat_mutex);
static DECLARE_WAIT_QUEUE_HEAD(bat_thread_wq);

////////////////////////////////////////////////////////////////////////////////
//Logging System
////////////////////////////////////////////////////////////////////////////////
int g_chr_event = 0;
int bat_volt_cp_flag = 0;
int bat_volt_check_point = 0;
int g_wake_up_bat=0;

void wake_up_bat (void)
{
	if (Enable_BATDRV_LOG == 1) {
		xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[BATTERY] wake_up_bat. \r\n");
	}

	g_wake_up_bat=1;

    bat_thread_timeout = 1;
    wake_up(&bat_thread_wq);
}
EXPORT_SYMBOL(wake_up_bat);

////////////////////////////////////////////////////////////////////////////////
// USB-IF
////////////////////////////////////////////////////////////////////////////////
int g_usb_state = USB_UNCONFIGURED;
int g_temp_CC_value = Cust_CC_500MA;
int g_soft_start_delay = 1;

#if (CONFIG_USB_IF == 0)
int g_Support_USBIF = 0;
#else
int g_Support_USBIF = 1;
#endif

////////////////////////////////////////////////////////////////////////////////
// Integrate with NVRAM
////////////////////////////////////////////////////////////////////////////////
#define ADC_CALI_DEVNAME "MT_pmic_adc_cali"

#define TEST_ADC_CALI_PRINT _IO('k', 0)
#define SET_ADC_CALI_Slop _IOW('k', 1, int)
#define SET_ADC_CALI_Offset _IOW('k', 2, int)
#define SET_ADC_CALI_Cal _IOW('k', 3, int)
#define ADC_CHANNEL_READ _IOW('k', 4, int)
#define BAT_STATUS_READ _IOW('k', 5, int)
#define Set_Charger_Current _IOW('k', 6, int)
#define AVG_BAT_SEN_READ _IOW('k', 7, int)
#define FGADC_CURRENT_READ _IOW('k', 8, int)
#define BAT_THREAD_CTRL _IOW('k', 9, int)

static struct class *adc_cali_class = NULL;
static int adc_cali_major = 0;
static dev_t adc_cali_devno;
static struct cdev *adc_cali_cdev;

int adc_cali_slop[14] = {1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000};
int adc_cali_offset[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int adc_cali_cal[1] = {0};

int adc_in_data[2] = {1,1};
int adc_out_data[2] = {1,1};

int battery_in_data[1] = {0};
int battery_out_data[1] = {0};

int charging_level_data[1] = {0};

int batsen_out_data[1] = {0};
int fgadc_out_data[1] = {0};

kal_bool g_ADC_Cali = KAL_FALSE;
kal_bool g_ftm_battery_flag = KAL_FALSE;

////////////////////////////////////////////////////////////////////////////////
// Battery Logging Entry
////////////////////////////////////////////////////////////////////////////////
static struct proc_dir_entry *proc_entry;
static char proc_bat_data[32];

static char proc_bat_data_ctrl[32];

int bat_thread_control(int start) //for psensor firmware download in factory mode
{
//    int  gpt_num = GPT5;

    if(start == 1)
    {
//        start_gpt(gpt_num);
        printk("Resume bat_thread_kthread after psensor firmware download\n");
    }
    else
    {
//        stop_gpt(gpt_num);
        printk("Stop bat_thread_kthread before psensor firmware download\n");
//        while(bat_thread_timeout == 1)
//        msleep(500);
        ncp1851_set_chg_en(0);
    }
    return 1;
}

ssize_t bat_log_write( struct file *filp, const char __user *buff,
                        unsigned long len, void *data )
{
	if (copy_from_user( &proc_bat_data, buff, len )) {
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "bat_log_write error.\n");
		return -EFAULT;
	}

	if (proc_bat_data[0] == '1') {
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "enable battery driver log system\n");
		Enable_BATDRV_LOG = 1;
	} else if (proc_bat_data[0] == '2') {
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "enable battery driver log system:2\n");
		Enable_BATDRV_LOG = 2;
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "Disable battery driver log system\n");
		Enable_BATDRV_LOG = 0;
	}

	return len;
}

ssize_t bat_thread_ctrl_write( struct file *filp, const char __user *buff,
                        unsigned long len, void *data )
{
	if (copy_from_user( &proc_bat_data_ctrl, buff, len )) {
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "bat_thread_ctrl_write error.\n");
		return -EFAULT;
	}

	if (proc_bat_data_ctrl[0] == '1') {
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "resume bat_thread_kthread()\n");
		bat_thread_control(1);
	} else if (proc_bat_data_ctrl[0] == '2') {
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "stop bat_thread_kthread()\n");
		bat_thread_control(0);
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "not valid value, resume bat_thread_kthread() as default\n");
		bat_thread_control(1);
	}

	return len;
}

int init_proc_log(void)
{
	int ret=0;
	proc_entry = create_proc_entry( "batdrv_log", 0644, NULL );

	if (proc_entry == NULL) {
		ret = -ENOMEM;
	  	xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "init_proc_log: Couldn't create proc entry\n");
	} else {
		proc_entry->write_proc = bat_log_write;
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "init_proc_log loaded.\n");
	}

	return ret;
}

////////////////////////////////////////////////////////////////////////////////
// FOR ANDROID BATTERY SERVICE
////////////////////////////////////////////////////////////////////////////////
struct mt6320_ac_data {
    struct power_supply psy;
    int AC_ONLINE;
};

struct mt6320_usb_data {
    struct power_supply psy;
    int USB_ONLINE;
};

struct mt6320_battery_data {
    struct power_supply psy;
    int BAT_STATUS;
    int BAT_HEALTH;
    int BAT_PRESENT;
    int BAT_TECHNOLOGY;
    int BAT_CAPACITY;
    /* Add for Battery Service*/
    int BAT_batt_vol;
    int BAT_batt_temp;
    /* Add for EM */
    int BAT_TemperatureR;
    int BAT_TempBattVoltage;
    int BAT_InstatVolt;
    int BAT_BatteryAverageCurrent;
    int BAT_BatterySenseVoltage;
    int BAT_ISenseVoltage;
    int BAT_ChargerVoltage;
};

static enum power_supply_property mt6320_ac_props[] = {
    POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property mt6320_usb_props[] = {
    POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property mt6320_battery_props[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_TECHNOLOGY,
    POWER_SUPPLY_PROP_CAPACITY,
    /* Add for Battery Service */
    POWER_SUPPLY_PROP_batt_vol,
    POWER_SUPPLY_PROP_batt_temp,
    /* Add for EM */
    POWER_SUPPLY_PROP_TemperatureR,
    POWER_SUPPLY_PROP_TempBattVoltage,
    POWER_SUPPLY_PROP_InstatVolt,
    POWER_SUPPLY_PROP_BatteryAverageCurrent,
    POWER_SUPPLY_PROP_BatterySenseVoltage,
    POWER_SUPPLY_PROP_ISenseVoltage,
    POWER_SUPPLY_PROP_ChargerVoltage,
};

static int mt6320_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
    int ret = 0;
    struct mt6320_ac_data *data = container_of(psy, struct mt6320_ac_data, psy);    

    switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	    val->intval = data->AC_ONLINE;
	    break;
	default:
	    ret = -EINVAL;
	    break;
    }
    return ret;
}

static int mt6320_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
    int ret = 0;
    struct mt6320_usb_data *data = container_of(psy, struct mt6320_usb_data, psy);    

    switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		#if defined(CONFIG_POWER_EXT)
		//#if 0
		data->USB_ONLINE = 1;
		val->intval = data->USB_ONLINE;
		#else
	    val->intval = data->USB_ONLINE;
		#endif
	    break;
	default:
	    ret = -EINVAL;
	    break;
    }
    return ret;
}

static int mt6320_battery_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
    int ret = 0;
    struct mt6320_battery_data *data = container_of(psy, struct mt6320_battery_data, psy);

    switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	    val->intval = data->BAT_STATUS;
	    break;
	case POWER_SUPPLY_PROP_HEALTH:
	    val->intval = data->BAT_HEALTH;
	    break;
	case POWER_SUPPLY_PROP_PRESENT:
	    val->intval = data->BAT_PRESENT;
	    break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
	    val->intval = data->BAT_TECHNOLOGY;
	    break;
	case POWER_SUPPLY_PROP_CAPACITY:
	    val->intval = data->BAT_CAPACITY;
	    break;
	case POWER_SUPPLY_PROP_batt_vol:
	    val->intval = data->BAT_batt_vol;
	    break;
	case POWER_SUPPLY_PROP_batt_temp:
	    val->intval = data->BAT_batt_temp;
	    break;
	case POWER_SUPPLY_PROP_TemperatureR:
	    val->intval = data->BAT_TemperatureR;
	    break;
	case POWER_SUPPLY_PROP_TempBattVoltage:
	    val->intval = data->BAT_TempBattVoltage;
	    break;
	case POWER_SUPPLY_PROP_InstatVolt:
	    val->intval = data->BAT_InstatVolt;
	    break;
	case POWER_SUPPLY_PROP_BatteryAverageCurrent:
	    val->intval = data->BAT_BatteryAverageCurrent;
	    break;
	case POWER_SUPPLY_PROP_BatterySenseVoltage:
	    val->intval = data->BAT_BatterySenseVoltage;
	    break;
	case POWER_SUPPLY_PROP_ISenseVoltage:
	    val->intval = data->BAT_ISenseVoltage;
	    break;
	case POWER_SUPPLY_PROP_ChargerVoltage:
	    val->intval = data->BAT_ChargerVoltage;
	    break;

	default:
	    ret = -EINVAL;
	    break;
    }

    return ret;
}

/* mt6320_ac_data initialization */
static struct mt6320_ac_data mt6320_ac_main = {
    .psy = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
    .properties = mt6320_ac_props,
    .num_properties = ARRAY_SIZE(mt6320_ac_props),
    .get_property = mt6320_ac_get_property,                
    },
    .AC_ONLINE = 0,
};

/* mt6320_usb_data initialization */
static struct mt6320_usb_data mt6320_usb_main = {
    .psy = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
    .properties = mt6320_usb_props,
    .num_properties = ARRAY_SIZE(mt6320_usb_props),
    .get_property = mt6320_usb_get_property,                
    },
    .USB_ONLINE = 0,
};

/* mt6320_battery_data initialization */
static struct mt6320_battery_data mt6320_battery_main = {
    .psy = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
    .properties = mt6320_battery_props,
    .num_properties = ARRAY_SIZE(mt6320_battery_props),
    .get_property = mt6320_battery_get_property,                
    },
/* CC: modify to have a full power supply status */
#if defined(CONFIG_POWER_EXT)
//#if 0
    .BAT_STATUS = POWER_SUPPLY_STATUS_FULL,
    .BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD,
    .BAT_PRESENT = 1,
    .BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION,
    .BAT_CAPACITY = 100,
    .BAT_batt_vol = 4200,
    .BAT_batt_temp = 22,
#else
    .BAT_STATUS = POWER_SUPPLY_STATUS_NOT_CHARGING,
    .BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD,
    .BAT_PRESENT = 1,
    .BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION,
    .BAT_CAPACITY = 50,
    .BAT_batt_vol = 0,
    .BAT_batt_temp = 0,
#endif
};

static int usb_is_discharging_det(void)
{
    int ret_val;
    if((BMT_status.charger_type == STANDARD_HOST) && (gFG_Is_Charging == KAL_FALSE) && (!g_Battery_Fail))
        ret_val = 1;
    else
        ret_val = 0;

    return ret_val;
}

#if defined(CONFIG_POWER_EXT)
#else
static void mt6320_ac_update(struct mt6320_ac_data *ac_data)
{
    struct power_supply *ac_psy = &ac_data->psy;

	if( upmu_is_chr_det() == KAL_TRUE )
	{
		if ( (BMT_status.charger_type == NONSTANDARD_CHARGER) ||
			 (BMT_status.charger_type == STANDARD_CHARGER)		)
		{
			ac_data->AC_ONLINE = 1;
			ac_psy->type = POWER_SUPPLY_TYPE_MAINS;
		}
	}
	else
	{
		ac_data->AC_ONLINE = 0;
	}

    power_supply_changed(ac_psy);
}

static void mt6320_usb_update(struct mt6320_usb_data *usb_data)
{
    struct power_supply *usb_psy = &usb_data->psy;

	if( upmu_is_chr_det() == KAL_TRUE )
    {
		if ( (BMT_status.charger_type == STANDARD_HOST) ||
			 (BMT_status.charger_type == CHARGING_HOST)		)
		{
		    usb_data->USB_ONLINE = 1;
		    usb_psy->type = POWER_SUPPLY_TYPE_USB;
		}
    }
    else
    {
		usb_data->USB_ONLINE = 0;
    }

    power_supply_changed(usb_psy);
}

extern int set_rtc_spare_fg_value(int val);

static void mt6320_battery_update(struct mt6320_battery_data *bat_data)
{
    struct power_supply *bat_psy = &bat_data->psy;
	int i;

    bat_data->BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
    bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
    bat_data->BAT_batt_vol = BMT_status.bat_vol;
    bat_data->BAT_batt_temp= BMT_status.temperature * 10;

    if (BMT_status.bat_exist)
        bat_data->BAT_PRESENT = 1;
    else
        bat_data->BAT_PRESENT = 0;

    /* Charger and Battery Exist */
    //if( (upmu_is_chr_det(CHR)==KAL_TRUE) && (!g_Battery_Fail) )
    if( (upmu_is_chr_det()==KAL_TRUE) && (!g_Battery_Fail) && (g_Charging_Over_Time==0) && (usb_is_discharging_det() == 0))
    {
        if ( BMT_status.bat_exist )
        {
            /* Battery Full */
#if defined(MTK_JEITA_STANDARD_SUPPORT)
            if ( (BMT_status.bat_vol >= g_jeita_recharging_voltage) && (BMT_status.bat_full == KAL_TRUE) )
#else
            if ( (BMT_status.bat_vol >= RECHARGING_VOLTAGE) && (BMT_status.bat_full == KAL_TRUE) )
#endif    			
            {
            	/*Use no gas gauge*/
				if( gForceADCsolution == 1 )
				{
	                bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_FULL;
	                bat_data->BAT_CAPACITY = Battery_Percent_100;

					/* For user view */
					for (i=0; i<BATTERY_AVERAGE_SIZE; i++) {
						batterySOCBuffer[i] = 100;
						batterySOCSum = 100 * BATTERY_AVERAGE_SIZE; /* for user view */
					}
					bat_volt_check_point = 100;
				}
				/*Use gas gauge*/
				else
				{
					gSyncPercentage=1;
#if defined(MTK_JEITA_STANDARD_SUPPORT)
                    //increase after xxs
                    if(gFGsyncTimer_jeita >= g_default_sync_time_out_jeita)
                    {
                        gFGsyncTimer_jeita=0;
                        bat_volt_check_point++;
                    }
                    else
                    {
                        gFGsyncTimer_jeita+=10;
                        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery] In JEITA (%d on %d)\r\n", 
                            bat_volt_check_point, gFGsyncTimer_jeita);
                    }
#else                    
					bat_volt_check_point++;
#endif       					
					if(bat_volt_check_point>=100)
					{
						bat_volt_check_point=100;
						bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_FULL;
					}
					bat_data->BAT_CAPACITY = bat_volt_check_point;

					if (Enable_BATDRV_LOG == 1) {
						xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery] In FULL Range (%d)\r\n", bat_volt_check_point);
					}

						gSyncPercentage=1;

						if (Enable_BATDRV_LOG == 1) {
							xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery_SyncRecharge] In recharging state, do not sync FG\r\n");
						}
				}
            }
            /* battery charging */
            else
            {
                /* Do re-charging for keep battery soc */
                if (g_bat_full_user_view)
                {
                    bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_FULL;
                    bat_data->BAT_CAPACITY = Battery_Percent_100;

					/* For user view */
					for (i=0; i<BATTERY_AVERAGE_SIZE; i++) {
						batterySOCBuffer[i] = 100;
						batterySOCSum = 100 * BATTERY_AVERAGE_SIZE; /* for user view */
					}
					bat_volt_check_point = 100;

					gSyncPercentage=1;
					if (Enable_BATDRV_LOG == 1) {
						xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery_Recharging] Keep UI as 100. bat_volt_check_point=%d, BMT_status.SOC=%d\r\n",
						bat_volt_check_point, BMT_status.SOC);
					}
                }
                else
                {
                    bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_CHARGING;

					/*Use no gas gauge*/
					if( gForceADCsolution == 1 )
					{
	                    /* SOC only UP when charging */
	                    if ( BMT_status.SOC > bat_volt_check_point ) {
							bat_volt_check_point = BMT_status.SOC;
	                    }
						bat_data->BAT_CAPACITY = bat_volt_check_point;
					}
					/*Use gas gauge*/
					else
					{
						if(bat_volt_check_point >= 100 )
						{
							bat_volt_check_point=99;
							//BMT_status.SOC=99;
							gSyncPercentage=1;

							//if (Enable_BATDRV_LOG == 1) {
								xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery] Use gas gauge : gas gague get 100 first (%d)\r\n", bat_volt_check_point);
							//}
						}
						else
						{
							if(bat_volt_check_point == BMT_status.SOC)
							{
								gSyncPercentage=0;

								if (Enable_BATDRV_LOG == 1) {
									xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery] Can sync due to bat_volt_check_point=%d, BMT_status.SOC=%d\r\n",
									bat_volt_check_point, BMT_status.SOC);
								}
							}
							else
							{
								if (Enable_BATDRV_LOG == 1) {
									xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery] Keep UI due to bat_volt_check_point=%d, BMT_status.SOC=%d\r\n",
									bat_volt_check_point, BMT_status.SOC);
								}
							}
						}
						bat_data->BAT_CAPACITY = bat_volt_check_point;
					}
                }
            }
        }
        /* No Battery, Only Charger */
        else
        {
            bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_UNKNOWN;
            bat_data->BAT_CAPACITY = 0;
        }

    }
    /* Only Battery */
    else
    {
        if(usb_is_discharging_det() == 1)
            bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_CHARGING;
        else
            bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_NOT_CHARGING;

        /* If VBAT < CLV, then shutdown */
        if (BMT_status.bat_vol <= SYSTEM_OFF_VOLTAGE)
        {
        	/*Use no gas gauge*/
			if( gForceADCsolution == 1 )
			{
	            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BAT BATTERY] VBAT < %d mV : Android will Power Off System !!\r\n", SYSTEM_OFF_VOLTAGE);
	            bat_data->BAT_CAPACITY = 0;
			}
			/*Use gas gauge*/
			else
			{
				gSyncPercentage=1;
				bat_volt_check_point--;
				if(bat_volt_check_point <= 0)
				{
					bat_volt_check_point=0;
				}
                                g_Calibration_FG = 0;
                                FGADC_Reset_SW_Parameter();
                                gFG_DOD0=100-bat_volt_check_point;
                                gFG_DOD1=gFG_DOD0;
                                BMT_status.SOC=bat_volt_check_point;
				bat_data->BAT_CAPACITY = bat_volt_check_point;
                                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery] VBAT < %d mV (%d, gFG_DOD0=%d)\r\n", SYSTEM_OFF_VOLTAGE, bat_volt_check_point,gFG_DOD0);                
			}
        }
		/* If FG_VBAT <= gFG_15_vlot, then run to 15% */
		else if ( (gFG_voltage <= gFG_15_vlot)&&(gForceADCsolution==0)&&(bat_volt_check_point>=15) )
		{
			/*Use gas gauge*/
			gSyncPercentage=1;
			if(gBAT_counter_15==0)
			{
				bat_volt_check_point--;
				gBAT_counter_15=1;
			}
			else
			{
				gBAT_counter_15=0;
			}
			g_Calibration_FG = 0;
    		FGADC_Reset_SW_Parameter();
			gFG_DOD0=100-bat_volt_check_point;
			gFG_DOD1=gFG_DOD0;
			BMT_status.SOC=bat_volt_check_point;
			bat_data->BAT_CAPACITY = bat_volt_check_point;
			xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery] FG_VBAT <= %d, then SOC run to 15. (SOC=%d,Point=%d,D1=%d,D0=%d)\r\n",
				gFG_15_vlot, BMT_status.SOC, bat_volt_check_point, gFG_DOD1, gFG_DOD0);
		}
		/* If "FG_VBAT > gFG_15_vlot" and "FG_report=15%" , then keep 15% till FG_VBAT <= gFG_15_vlot */
		else if ( (gFG_voltage > gFG_15_vlot)&&(gForceADCsolution==0)&&(bat_volt_check_point==15) )
		{
			/*Use gas gauge*/
			gSyncPercentage=1;
			gBAT_counter_15=1;
			g_Calibration_FG = 0;
    		FGADC_Reset_SW_Parameter();
			gFG_DOD0=100-bat_volt_check_point;
			gFG_DOD1=gFG_DOD0;
			BMT_status.SOC=bat_volt_check_point;
			bat_data->BAT_CAPACITY = bat_volt_check_point;
			xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery] FG_VBAT(%d) > gFG_15_vlot(%d) and FG_report=15, then UI(%d) keep 15. (D1=%d,D0=%d)\r\n",
				gFG_voltage, gFG_15_vlot, bat_volt_check_point, gFG_DOD1, gFG_DOD0);
		}
        else
        {
        	gBAT_counter_15=1;
			/*Use no gas gauge*/
			if( gForceADCsolution == 1 )
			{
				/* SOC only Done when dis-charging */
	            if ( BMT_status.SOC < bat_volt_check_point ) {
					bat_volt_check_point = BMT_status.SOC;
	            }
				bat_data->BAT_CAPACITY = bat_volt_check_point;
			}
			/*Use gas gauge : gas gague get 0% fist*/
			else
			{
				if (Enable_BATDRV_LOG == 1) {
					xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery_OnlyBattery!] bat_volt_check_point=%d,BMT_status.SOC=%d\r\n",
					bat_volt_check_point, BMT_status.SOC);
				}

				//if(bat_volt_check_point != BMT_status.SOC)
				//if(bat_volt_check_point > BMT_status.SOC)
				if( (bat_volt_check_point>BMT_status.SOC) && ((bat_volt_check_point!=1)) )
				{
					if (Enable_BATDRV_LOG == 1) {
						xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery_OnlyBattery] bat_volt_check_point=%d,BMT_status.SOC=%d,gFGsyncTimer=%d(on %d)\r\n",
						bat_volt_check_point, BMT_status.SOC, gFGsyncTimer, DEFAULT_SYNC_TIME_OUT);
					}

					//reduce after xxs
					if(gFGsyncTimer >= DEFAULT_SYNC_TIME_OUT)
					{
						gFGsyncTimer=0;
						bat_volt_check_point--;
						bat_data->BAT_CAPACITY = bat_volt_check_point;
					}
					else
					{
						gFGsyncTimer+=10;
					}
				}
				else
				{
					if(bat_volt_check_point <= 0 )
					{
						bat_volt_check_point=1;
						//BMT_status.SOC=1;
						gSyncPercentage=1;

						//if (Enable_BATDRV_LOG == 1) {
							xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery] Use gas gauge : gas gague get 0 first (%d)\r\n", bat_volt_check_point);
						//}
					}
					else
					{
						gSyncPercentage=0;
					}

					if(bat_volt_check_point > 100)
					{
						bat_volt_check_point=100;
					}

					bat_data->BAT_CAPACITY = bat_volt_check_point;
				}

				if(bat_volt_check_point == 100) {
        			g_bat_full_user_view = KAL_TRUE;
					if (Enable_BATDRV_LOG == 1) {
						xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery_Only] Set g_bat_full_user_view=KAL_TRUE\r\n");
					}
				}
			}
        }
    }

	if (Enable_BATDRV_LOG >= 1) {
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:IntegrationFG:point,per_C,per_V,count,vbat_charger,CSDAC_DAT] %d,%d,%d,%d,%d,ADC_Solution=%d\r\n",
		bat_volt_check_point, BMT_status.SOC, FGADC_Get_BatteryCapacity_VoltageMothod(),
		BATTERY_AVERAGE_SIZE, BMT_status.bat_vol, gForceADCsolution);
	}

	/* Update for EM */
	bat_data->BAT_TemperatureR=g_BAT_TemperatureR;
	bat_data->BAT_TempBattVoltage=g_TempBattVoltage;
	bat_data->BAT_InstatVolt=g_InstatVolt;
	bat_data->BAT_BatteryAverageCurrent=g_BatteryAverageCurrent;
	bat_data->BAT_BatterySenseVoltage=g_BAT_BatterySenseVoltage;
	bat_data->BAT_ISenseVoltage=g_BAT_ISenseVoltage;
	bat_data->BAT_ChargerVoltage=g_BAT_ChargerVoltage;

	if (gFG_booting_counter_I_FLAG == 2) {
		set_rtc_spare_fg_value(bat_volt_check_point);
	}

    power_supply_changed(bat_psy);
}

static void mt6320_battery_update_power_down(struct mt6320_battery_data *bat_data)
{
    struct power_supply *bat_psy = &bat_data->psy;

    bat_data->BAT_CAPACITY = 0;

    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[Battery] mt6320_battery_update_power_down\r\n");

    power_supply_changed(bat_psy);
}
#endif

#if defined(CONFIG_POWER_VERIFY)

void BATTERY_SetUSBState(int usb_state_value)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[BATTERY_SetUSBState] in FPGA/EVB, no service\r\n");
}
EXPORT_SYMBOL(BATTERY_SetUSBState);

static int mt6320_battery_probe(struct platform_device *dev)    
{
    int ret=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "******** MT6320 battery driver probe!! ********\n" );
        
    /* Integrate with Android Battery Service */
    ret = power_supply_register(&(dev->dev), &mt6320_ac_main.psy);
    if (ret)
    {            
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[MT6320 BAT_probe] power_supply_register AC Fail !!\n");                    
        return ret;
    }             
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[MT6320 BAT_probe] power_supply_register AC Success !!\n");

    ret = power_supply_register(&(dev->dev), &mt6320_usb_main.psy);
    if (ret)
    {            
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[MT6320 BAT_probe] power_supply_register USB Fail !!\n");                    
        return ret;
    }             
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[MT6320 BAT_probe] power_supply_register USB Success !!\n");

    ret = power_supply_register(&(dev->dev), &mt6320_battery_main.psy);
    if (ret)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[MT6320 BAT_probe] power_supply_register Battery Fail !!\n");
        return ret;
    }
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[MT6320 BAT_probe] power_supply_register Battery Success !!\n");

    //battery kernel thread for 10s check and charger in/out event
    /*kthread_run(bat_thread_kthread, NULL, "bat_thread_kthread"); */
    //xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "[bat_thread_kthread] Done\n");

	g_bat_init_flag=1;
    
    return 0;
}

#else
///////////////////////////////////////////////////////////////////////////////////////////
//// Battery Temprature Parameters and functions
///////////////////////////////////////////////////////////////////////////////////////////
typedef struct{
    INT32 BatteryTemp;
    INT32 TemperatureR;
}BATT_TEMPERATURE;

/* convert register to temperature  */
INT16 BattThermistorConverTemp(INT32 Res)
{
    int i=0;
    INT32 RES1=0,RES2=0;
    INT32 TBatt_Value=-200,TMP1=0,TMP2=0;

#if defined(BAT_NTC_10_TDK_1)        
    BATT_TEMPERATURE Batt_Temperature_Table[] = {
     {-20,95327},
     {-15,71746},
     {-10,54564},
     { -5,41813},
     {  0,32330},
     {  5,25194},
     { 10,19785},
     { 15,15651},
     { 20,12468},
     { 25,10000},
     { 30,8072},
     { 35,6556},
     { 40,5356},
     { 45,4401},
     { 50,3635},
     { 55,3019},
     { 60,2521}
    };
#endif

#if defined(BAT_NTC_TSM_1)
    BATT_TEMPERATURE Batt_Temperature_Table[] = {
    {-20,70603},    
    {-15,55183},
    {-10,43499},
    { -5,34569},
    {  0,27680},
    {  5,22316},
    { 10,18104},
    { 15,14773},
    { 20,12122},
    { 25,10000},
    { 30,8294},
    { 35,6915},
    { 40,5795},
    { 45,4882},
    { 50,4133},
    { 55,3516},
    { 60,3004}
    };
#endif

#if defined(BAT_NTC_10_SEN_1)        
    BATT_TEMPERATURE Batt_Temperature_Table[] = {
     {-20,74354},
     {-15,57626},
     {-10,45068},
     { -5,35548},
     {  0,28267},
     {  5,22650},
     { 10,18280},
     { 15,14855},
     { 20,12151},
     { 25,10000},
     { 30,8279},
     { 35,6892},
     { 40,5768},
     { 45,4852},
     { 50,4101},
     { 55,3483},
     { 60,2970}
    };
#endif

#if (BAT_NTC_10 == 1)
    BATT_TEMPERATURE Batt_Temperature_Table[] = {
        {-20,68237},
        {-15,53650},
        {-10,42506},
        { -5,33892},
        {  0,27219},
        {  5,22021},
        { 10,17926},
        { 15,14674},
        { 20,12081},
        { 25,10000},
        { 30,8315},
        { 35,6948},
        { 40,5834},
        { 45,4917},
        { 50,4161},
        { 55,3535},
        { 60,3014}
    };
#endif

#if (BAT_NTC_47 == 1)
	BATT_TEMPERATURE Batt_Temperature_Table[] = {
		{-20,483954},
		{-15,360850},
		{-10,271697},
		{ -5,206463},
		{  0,158214},
		{  5,122259},
		{ 10,95227},
		{ 15,74730},
		{ 20,59065},
		{ 25,47000},
		{ 30,37643},
		{ 35,30334},
		{ 40,24591},
		{ 45,20048},
		{ 50,16433},
		{ 55,13539},
		{ 60,11210}
	};
#endif

    if(Res>=Batt_Temperature_Table[0].TemperatureR)
    {
        #if 0
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "Res>=%d\n", Batt_Temperature_Table[0].TemperatureR);
        #endif
        TBatt_Value = -20;
    }
    else if(Res<=Batt_Temperature_Table[16].TemperatureR)
    {
        #if 0
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "Res<=%d\n", Batt_Temperature_Table[16].TemperatureR);
        #endif
        TBatt_Value = 60;
    }
    else
    {
        RES1=Batt_Temperature_Table[0].TemperatureR;
        TMP1=Batt_Temperature_Table[0].BatteryTemp;

        for(i=0;i<=16;i++)
        {
            if(Res>=Batt_Temperature_Table[i].TemperatureR)
            {
                RES2=Batt_Temperature_Table[i].TemperatureR;
                TMP2=Batt_Temperature_Table[i].BatteryTemp;
                break;
            }
            else
            {
                RES1=Batt_Temperature_Table[i].TemperatureR;
                TMP1=Batt_Temperature_Table[i].BatteryTemp;
            }
        }

        TBatt_Value = (((Res-RES2)*TMP1)+((RES1-Res)*TMP2))/(RES1-RES2);
    }

    #if 0
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "BattThermistorConverTemp() : TBatt_Value = %d\n",TBatt_Value);
	xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "BattThermistorConverTemp() : Res = %d\n",Res);
	xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "BattThermistorConverTemp() : RES1 = %d\n",RES1);
	xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "BattThermistorConverTemp() : RES2 = %d\n",RES2);
	xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "BattThermistorConverTemp() : TMP1 = %d\n",TMP1);
	xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "BattThermistorConverTemp() : TMP2 = %d\n",TMP2);
    #endif

    return TBatt_Value;
}

/* convert ADC_bat_temp_volt to register */
INT16 BattVoltToTemp(UINT32 dwVolt)
{
    INT32 TRes;
    INT32 dwVCriBat = (TBAT_OVER_CRITICAL_LOW*RBAT_PULL_UP_VOLT)/(TBAT_OVER_CRITICAL_LOW+RBAT_PULL_UP_R);
    INT32 sBaTTMP = -100;

    if(dwVolt > dwVCriBat)
    {
        TRes = TBAT_OVER_CRITICAL_LOW;
    }
    else
    {
		TRes = (RBAT_PULL_UP_R*dwVolt)/(RBAT_PULL_UP_VOLT-dwVolt);
    }

	g_BAT_TemperatureR = TRes;

    /* convert register to temperature */
    sBaTTMP = BattThermistorConverTemp(TRes);

    #if 0
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "BattVoltToTemp() : TBAT_OVER_CRITICAL_LOW = %d\n", TBAT_OVER_CRITICAL_LOW);
	xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "BattVoltToTemp() : RBAT_PULL_UP_VOLT = %d\n", RBAT_PULL_UP_VOLT);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "BattVoltToTemp() : dwVolt = %d\n", dwVolt);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "BattVoltToTemp() : TRes = %d\n", TRes);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "BattVoltToTemp() : sBaTTMP = %d\n", sBaTTMP);
    #endif

//    if(upmu_get_cid() == 0x1020)
//        sBaTTMP=22; 

    return sBaTTMP;
}

//void BAT_SetUSBState(int usb_state_value)
void BATTERY_SetUSBState(int usb_state_value)
{
	if ( (usb_state_value < USB_SUSPEND) || ((usb_state_value > USB_CONFIGURED))){
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY] BAT_SetUSBState Fail! Restore to default value\r\n");
		usb_state_value = USB_UNCONFIGURED;
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY] BAT_SetUSBState Success! Set %d\r\n", usb_state_value);
		g_usb_state = usb_state_value;
	}
}
//EXPORT_SYMBOL(BAT_SetUSBState);
EXPORT_SYMBOL(BATTERY_SetUSBState);

kal_bool pmic_chrdet_status(void)
{
    if( upmu_is_chr_det() == KAL_TRUE )
    {
        return KAL_TRUE;
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[pmic_chrdet_status] No charger\r\n");
        return KAL_FALSE;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
//// External charger
///////////////////////////////////////////////////////////////////////////////////////////
int boot_soc_temp=0;
int boot_soc_temp_count=0;
int boot_check_once=1;

int g_bcct_flag=0;
int g_bcct_value=0;

void select_charging_curret_bcct(void)
{
    if (BMT_status.charger_type == STANDARD_HOST)
    {
        if(g_bcct_value < 100)        
            g_temp_CC_value = Cust_CC_0MA;
        else if(g_bcct_value < 500)
        	  g_temp_CC_value = Cust_CC_400MA;
        else
        	  g_temp_CC_value = Cust_CC_500MA;    
        	  
        ncp1851_set_iinlim(0x1); //IN current limit at 500mA		  
        ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
        ncp1851_set_iinlim_en(0x1); //enable input current limit
        ncp1851_set_aicl_en(0x0); //disable AICL        	  
        if(g_temp_CC_value != Cust_CC_0MA)    	
            ncp1851_set_ichg(g_temp_CC_value);
    }
    else if( (BMT_status.charger_type == STANDARD_CHARGER) ||
             (BMT_status.charger_type == CHARGING_HOST) ||
             (BMT_status.charger_type == NONSTANDARD_CHARGER))
    {
        //---------------------------------------------------
        //set IOCHARGE
        if(g_bcct_value < 100)
        {
            g_temp_CC_value = Cust_CC_0MA;     	
        }
        else if(g_bcct_value < 500)        
        {
            g_temp_CC_value = Cust_CC_400MA;
            ncp1851_set_iinlim(0x1); //IN current limit at 500mA		  
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
            ncp1851_set_iinlim_en(0x1); //enable input current limit
            ncp1851_set_aicl_en(0x0); //disable AICL
            ncp1851_set_ichg(g_temp_CC_value);       	
        }
        else if(g_bcct_value < 600)        
        {
            g_temp_CC_value = Cust_CC_500MA;
            ncp1851_set_iinlim(0x1); //IN current limit at 500mA		  
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
            ncp1851_set_iinlim_en(0x1); //enable input current limit
            ncp1851_set_aicl_en(0x0); //disable AICL
            ncp1851_set_ichg(g_temp_CC_value);       	
        }        
        else if(g_bcct_value < 700)        
        {
            g_temp_CC_value = Cust_CC_600MA;
            ncp1851_set_iinlim(0x2); //IN current limit at 900mA		  
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
            ncp1851_set_iinlim_en(0x1); //enable input current limit
            ncp1851_set_aicl_en(0x1); //disable AICL
            ncp1851_set_ichg(g_temp_CC_value);       	
        }    
        else if(g_bcct_value < 800)        
        {
            g_temp_CC_value = Cust_CC_700MA;
            ncp1851_set_iinlim(0x2); //IN current limit at 900mA		  
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
            ncp1851_set_iinlim_en(0x1); //enable input current limit
            ncp1851_set_aicl_en(0x1); //disable AICL
            ncp1851_set_ichg(g_temp_CC_value);       	
        }           
        else if(g_bcct_value < 900)        
        {
            g_temp_CC_value = Cust_CC_800MA;
            ncp1851_set_iinlim(0x2); //IN current limit at 900mA		  
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
            ncp1851_set_iinlim_en(0x1); //enable input current limit
            ncp1851_set_aicl_en(0x1); //disable AICL
            ncp1851_set_ichg(g_temp_CC_value);       	
        }             
        else if(g_bcct_value < 1000)        
        {
            g_temp_CC_value = Cust_CC_900MA;
            ncp1851_set_iinlim(0x3); //IN current limit at 1500mA		  
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
            ncp1851_set_iinlim_en(0x1); //enable input current limit
            ncp1851_set_aicl_en(0x1); //disable AICL
            ncp1851_set_ichg(g_temp_CC_value);       	
        }            
        else if(g_bcct_value < 1100)        
        {
            g_temp_CC_value = Cust_CC_1000MA;
            ncp1851_set_iinlim(0x3); //IN current limit at 1500mA		  
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
            ncp1851_set_iinlim_en(0x1); //enable input current limit
            ncp1851_set_aicl_en(0x1); //disable AICL
            ncp1851_set_ichg(g_temp_CC_value);       	
        }        
        else if(g_bcct_value < 1200)        
        {
            g_temp_CC_value = Cust_CC_1100MA;
            ncp1851_set_iinlim(0x3); //IN current limit at 1500mA		  
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
            ncp1851_set_iinlim_en(0x1); //enable input current limit
            ncp1851_set_aicl_en(0x1); //disable AICL
            ncp1851_set_ichg(g_temp_CC_value);       	
        }          
        else if(g_bcct_value < 1300)        
        {
            g_temp_CC_value = Cust_CC_1200MA;
            ncp1851_set_iinlim(0x3); //IN current limit at 1500mA		  
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
            ncp1851_set_iinlim_en(0x1); //enable input current limit
            ncp1851_set_aicl_en(0x1); //disable AICL
            ncp1851_set_ichg(g_temp_CC_value);       	
        }         
        else if(g_bcct_value < 1400)        
        {
            g_temp_CC_value = Cust_CC_1300MA;
            ncp1851_set_iinlim(0x3); //IN current limit at 1500mA		  
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
            ncp1851_set_iinlim_en(0x1); //enable input current limit
            ncp1851_set_aicl_en(0x1); //disable AICL
            ncp1851_set_ichg(g_temp_CC_value);       	
        }         
        else if(g_bcct_value < 1500)        
        {
            g_temp_CC_value = Cust_CC_1400MA;
            ncp1851_set_iinlim(0x3); //IN current limit at 1500mA		  
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
            ncp1851_set_iinlim_en(0x1); //enable input current limit
            ncp1851_set_aicl_en(0x1); //disable AICL
            ncp1851_set_ichg(g_temp_CC_value);       	
        }         
        else if(g_bcct_value < 1600)        
        {
            g_temp_CC_value = Cust_CC_1500MA;
            ncp1851_set_iinlim(0x3); //IN current limit at 1500mA		  
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
            ncp1851_set_iinlim_en(0x1); //enable input current limit
            ncp1851_set_aicl_en(0x1); //disable AICL
            ncp1851_set_ichg(g_temp_CC_value);       	
        }                 
        else
        {
            g_temp_CC_value = Cust_CC_1600MA;
            ncp1851_set_iinlim(0x3); //IN current limit at 1500mA		  
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
            ncp1851_set_iinlim_en(0x1); //enable input current limit
            ncp1851_set_aicl_en(0x1); //disable AICL
            ncp1851_set_ichg(g_temp_CC_value);       	
        }             	
        //---------------------------------------------------
    }
    else
    {
            g_temp_CC_value = Cust_CC_500MA;   
            ncp1851_set_iinlim(0x1); //IN current limit at 500mA		  
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
            ncp1851_set_iinlim_en(0x1); //enable input current limit
            ncp1851_set_aicl_en(0x0); //disable AICL
            ncp1851_set_ichg(g_temp_CC_value);                 
    } 
}

extern kal_uint32 ncp1851_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT);

int get_bat_charging_current_level(void)
{
    kal_uint8 ret_val=0;    
    
    if (BMT_status.charger_type == STANDARD_HOST)
    {
        //Get current level
        ncp1851_read_interface(0xF, &ret_val, 0xF, 0x0);
        
        //Parsing
        if(ret_val==0x00)         return 400;
        else if(ret_val==0x01)    return 500;
        else                      return -1;           
    }
    else if((BMT_status.charger_type == STANDARD_CHARGER) ||
             (BMT_status.charger_type == CHARGING_HOST) ||
             (BMT_status.charger_type == NONSTANDARD_CHARGER))
    {
        //Get current level
        ncp1851_read_interface(0xF, &ret_val, 0xF, 0x0);
        
        //Parsing
        if(ret_val==0x00)         return  400;
        else if(ret_val==0x01)    return  500;
        else if(ret_val==0x02)    return  600;
        else if(ret_val==0x03)    return  700;
        else if(ret_val==0x04)    return  800;
        else if(ret_val==0x05)    return  900;
        else if(ret_val==0x06)    return 1000;
        else if(ret_val==0x07)    return 1100;
        else if(ret_val==0x08)    return 1200;        	
        else if(ret_val==0x09)    return 1300;        	
        else if(ret_val==0x0A)    return 1400;        	
        else if(ret_val==0x0B)    return 1500;        	
        else if(ret_val==0x0C)    return 1600;        	        	
        else                      return   -1;
    }
    else
    {
        return -1;
    }
}

int set_bat_charging_current_limit(int current_limit)
{
    printk("[BATTERY:fan5405] set_bat_charging_current_limit (%d)\r\n", current_limit);

    if(current_limit != -1)
    {
        g_bcct_flag=1;
                
        g_bcct_value=current_limit;      
    }
    else
    {
        //change to default current setting
        g_bcct_flag=0;
    }
    
    wake_up_bat();

    return g_bcct_flag;
}   

int g_current_change_flag = 0;

void ncp1851_set_ac_current(void)
{
    if (Enable_BATDRV_LOG == 1) {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] ncp1851_set_ac_current \r\n");
    }

#ifdef MTK_NCP1852_SUPPORT
    if((g_temp_CC_value > Cust_CC_1800MA) || (g_temp_CC_value < Cust_CC_400MA))
#else
    if((g_temp_CC_value > Cust_CC_1600MA) || (g_temp_CC_value < Cust_CC_400MA))
#endif		
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] invalid current selected (%ld), use 500mA \r\n", g_temp_CC_value);
	 ncp1851_set_ichg(1);
    }
    else
    {
#if defined(MTK_JEITA_STANDARD_SUPPORT)    
        if(g_temp_status == TEMP_NEG_10_TO_POS_0)
        {    
            ncp1851_set_ichg(Cust_CC_500MA);
        }  
        else
        {
            ncp1851_set_ichg(g_temp_CC_value);
        }
#else
        if(g_current_change_flag)
            ncp1851_set_ichg(Cust_CC_400MA);
        else
            ncp1851_set_ichg(g_temp_CC_value);  
#endif
    }
}

void ncp1851_set_low_chg_current(void)
{
    if (Enable_BATDRV_LOG == 1) {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] ncp1851_set_low_chg_current \r\n");
    }
    ncp1851_set_iinlim(0x0); //IN current limit at 100mA	
    ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
    ncp1851_set_iinlim_en(0x1); //enable I2C input current limit
    ncp1851_set_aicl_en(0x0); //disable AICL
    ncp1851_set_ichg(0x0); //charging current limit at 400mA
}

void select_charging_curret_ncp1851(void)
{
    if (g_ftm_battery_flag)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] FTM charging : %d\r\n", charging_level_data[0]);
        g_temp_CC_value = charging_level_data[0];

#ifdef MTK_NCP1852_SUPPORT
        if((g_temp_CC_value > Cust_CC_1800MA) || (g_temp_CC_value < Cust_CC_400MA))
#else
        if((g_temp_CC_value > Cust_CC_1600MA) || (g_temp_CC_value < Cust_CC_400MA))
#endif
        {
            ncp1851_set_low_chg_current(); //100mA
        }
        else
        {
            ncp1851_set_ac_current();
        }
    }
    else
    {
        if ( BMT_status.charger_type == STANDARD_HOST )
        {
            if (g_Support_USBIF == 1)
            {
                if (g_usb_state == USB_SUSPEND)
                {
                    g_temp_CC_value = USB_CHARGER_CURRENT_SUSPEND;
                    if (Enable_BATDRV_LOG == 1) {
                        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] disable charging\r\n");
                    }
                }
                else if (g_usb_state == USB_UNCONFIGURED)
                {
                    g_temp_CC_value = USB_CHARGER_CURRENT_UNCONFIGURED;
                    ncp1851_set_low_chg_current(); // 100mA
                    if (Enable_BATDRV_LOG == 1) {
                        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] ncp1851_set_low_chg_current() \r\n");
                    }
                }
                else if (g_usb_state == USB_CONFIGURED)
                {
                    g_temp_CC_value = USB_CHARGER_CURRENT_CONFIGURED;
                    ncp1851_set_iinlim(0x1); //IN current limit at 500mA					
                    ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
                    ncp1851_set_iinlim_en(0x1); //enable input current limit
                    ncp1851_set_aicl_en(0x0); //disable AICL
                    ncp1851_set_ac_current();
                    if (Enable_BATDRV_LOG == 1) {
                        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] ncp1851_set_ac_current(), CC value (%ld) \r\n", g_temp_CC_value);
                    }
                }
                else
                {
                    g_temp_CC_value = USB_CHARGER_CURRENT_UNCONFIGURED;
                    ncp1851_set_low_chg_current(); // 100mA
                    if (Enable_BATDRV_LOG == 1) {
                        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] ncp1851_set_low_chg_current() \r\n");
                    }
                }
            }
            else
            {
                g_temp_CC_value = USB_CHARGER_CURRENT;
                ncp1851_set_iinlim(0x1); //IN current limit at 500mA		  
                ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
                ncp1851_set_iinlim_en(0x1); //enable input current limit
                ncp1851_set_aicl_en(0x0); //disable AICL
                ncp1851_set_ac_current();
                if (Enable_BATDRV_LOG == 1) {
                    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] ncp1851_set_ac_current(), CC value(%ld) \r\n", g_temp_CC_value);
                }
            }
        }
        else if (BMT_status.charger_type == NONSTANDARD_CHARGER)
        {
            g_temp_CC_value = AC_CHARGER_CURRENT;
            ncp1851_set_iinlim(0x3); //IN current limit at 1.5A			
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
#ifdef MTK_NCP1852_SUPPORT            
            if((g_temp_CC_value > Cust_CC_1500MA)&& (g_temp_CC_value <= Cust_CC_1800MA))
                ncp1851_set_iinlim_en(0x0); //disable input current limit
            else
                ncp1851_set_iinlim_en(0x1); //enable input current limit				
#else
            ncp1851_set_iinlim_en(0x1); //enable input current limit
#endif            
            ncp1851_set_aicl_en(0x1); //enable AICL
            ncp1851_set_ac_current();
        }
        else if (BMT_status.charger_type == STANDARD_CHARGER)
        {
            g_temp_CC_value = AC_CHARGER_CURRENT;
            ncp1851_set_iinlim(0x3); //IN current limit at 1.5A			
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
#ifdef MTK_NCP1852_SUPPORT            
            if((g_temp_CC_value > Cust_CC_1500MA)&& (g_temp_CC_value <= Cust_CC_1800MA))
                ncp1851_set_iinlim_en(0x0); //disable input current limit
            else
            ncp1851_set_iinlim_en(0x1); //enable input current limit
#else            
            ncp1851_set_iinlim_en(0x1); //enable input current limit
#endif            
            ncp1851_set_aicl_en(0x1); //enable AICL
            ncp1851_set_ac_current();
        }
        else if (BMT_status.charger_type == CHARGING_HOST)
        {
            g_temp_CC_value = AC_CHARGER_CURRENT;
            ncp1851_set_iinlim(0x3); //IN current limit at 1.5A			
            ncp1851_set_iinset_pin_en(0x0); //Input current limit and AICL control by I2C
#ifdef MTK_NCP1852_SUPPORT            
            if((g_temp_CC_value > Cust_CC_1500MA) && (g_temp_CC_value <= Cust_CC_1800MA))
                ncp1851_set_iinlim_en(0x0); //disable input current limit
            else
            ncp1851_set_iinlim_en(0x1); //enable input current limit
#else                  
            ncp1851_set_iinlim_en(0x1); //enable input current limit
#endif            
            ncp1851_set_aicl_en(0x1); //enable AICL
            ncp1851_set_ac_current();
        }
        else
        {
            if (Enable_BATDRV_LOG == 1) {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Unknown charger type\n");
            }
            g_temp_CC_value = Cust_CC_100MA;
            ncp1851_set_low_chg_current();
        }
    }
}

void ChargerHwInit_ncp1851(void)
{
    kal_uint32 ncp1851_status;

    if (Enable_BATDRV_LOG == 1) {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] ChargerHwInit_ncp1851\n" );
    }

    ncp1851_status = ncp1851_get_chip_status();

    upmu_set_rg_bc11_bb_ctrl(1);    //BC11_BB_CTRL    
    upmu_set_rg_bc11_rst(1);        //BC11_RST

    ncp1851_set_otg_en(0x0);
    ncp1851_set_ntc_en(0x0);
    ncp1851_set_trans_en(0);
    ncp1851_set_tj_warn_opt(0x1);
    ncp1851_set_jeita_opt(0x1);
//  ncp1851_set_int_mask(0x0); //disable all interrupt
    ncp1851_set_int_mask(0x1); //enable all interrupt for boost mode status monitor
    ncp1851_set_tchg_rst(0x1); //reset charge timer
#ifdef NCP1851_PWR_PATH
    ncp1851_set_pwr_path(0x1);
#else
    ncp1851_set_pwr_path(0x0);
#endif

#if defined(MTK_JEITA_STANDARD_SUPPORT)        
    if(g_temp_status == TEMP_NEG_10_TO_POS_0)
    {    
        ncp1851_set_ctrl_vbat(0x1C); //VCHG = 4.0V
    }
    else
    {
        if(g_temp_status == TEMP_POS_10_TO_POS_45)
        {
            if((ncp1851_status == 0x8) || (ncp1851_status == 0x9) || (ncp1851_status == 0xA)) //WEAK WAIT, WEAK SAFE, WEAK CHARGE
                ncp1851_set_ctrl_vbat(0x1C); //VCHG = 4.0V
            else if(ncp1851_status == 0x4)
                ncp1851_set_ctrl_vbat(0x28); //VCHG = 4.3V to decrease charge time
            else
                ncp1851_set_ctrl_vbat(0x24); //VCHG = 4.2V
        }
        else
            ncp1851_set_ctrl_vbat(0x20); //VCHG = 4.1V
    }     
#else
    if((ncp1851_status == 0x8) || (ncp1851_status == 0x9) || (ncp1851_status == 0xA)) //WEAK WAIT, WEAK SAFE, WEAK CHARGE
        ncp1851_set_ctrl_vbat(0x1C); //VCHG = 4.0V
    else if(ncp1851_status == 0x4)
        ncp1851_set_ctrl_vbat(0x28); //VCHG = 4.3V to decrease charge time
    else
        ncp1851_set_ctrl_vbat(0x24); //VCHG = 4.2V
#endif    

    ncp1851_set_ieoc(0x4); // terminate current = 200mA for ICS optimized suspend power

    if((BMT_status.charger_type != STANDARD_HOST) && (BMT_status.charger_type != CHARGER_UNKNOWN))
        ncp1851_set_iweak(0x3); //weak charge current = 300mA
    else
        ncp1851_set_iweak(0x1); //weak charge current = 100mA

    ncp1851_set_ctrl_vfet(0x3); // VFET = 3.4V
    ncp1851_set_vchred(0x2); //reduce 200mV (JEITA)
    ncp1851_set_ichred(0x0); //reduce 400mA (JEITA)
    ncp1851_set_batcold(0x5);
    ncp1851_set_bathot(0x3);
    ncp1851_set_batchilly(0x0);
    ncp1851_set_batwarm(0x0);
}

void pchr_turn_off_charging_ncp1851 (void)
{
	if (Enable_BATDRV_LOG == 1) {
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] pchr_turn_off_charging_ncp1851!\r\n");
	}

	ncp1851_set_chg_en(0x0);
}

void pchr_turn_on_charging_ncp1851 (void)
{
	if ( BMT_status.bat_charging_state == CHR_ERROR )
	{
		if (Enable_BATDRV_LOG == 1) {
			xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Charger Error, turn OFF charging !\r\n");
		}
		BMT_status.total_charging_time = 0;
		pchr_turn_off_charging_ncp1851();
	}
  else if( (get_boot_mode()==META_BOOT) || (get_boot_mode()==ADVMETA_BOOT) )
  {   
      printk("[BATTERY:ncp1851] In meta or advanced meta mode, disable charging.\r\n");    
      pchr_turn_off_charging_ncp1851();
  }		
	else
	{
 		ChargerHwInit_ncp1851();

		if (Enable_BATDRV_LOG == 1) {
			xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] pchr_turn_on_charging_ncp1851 !\r\n");
		}

    if (g_bcct_flag == 1)
    {
        select_charging_curret_bcct();
        if (Enable_BATDRV_LOG == 1) {
            printk("[BATTERY:ncp1851] select_charging_curret_bcct !\n");
        }
    }
    else
    {
		    select_charging_curret_ncp1851();
        if (Enable_BATDRV_LOG == 1) {
            printk("[BATTERY:ncp1851] select_charging_curret_ncp1851 !\n");
        }
    }
        
		if( g_temp_CC_value == Cust_CC_0MA)
		{
		    pchr_turn_off_charging_ncp1851();
		}
        else
        {
            if((gFG_booting_counter_I_FLAG == 2) || (g_ocv_lookup_done == 1))
            {
                ncp1851_set_chg_en(0x1); // charger enable
                //Set SPM = 1, for lenovo platform, GPIO_CHR_SPM_PIN
                mt_set_gpio_mode(GPIO_CHR_SPM_PIN, GPIO_MODE_00);
                mt_set_gpio_dir(GPIO_CHR_SPM_PIN, GPIO_DIR_OUT);
                mt_set_gpio_out(GPIO_CHR_SPM_PIN, GPIO_OUT_ONE);
                if (Enable_BATDRV_LOG == 1) {
                    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] charger enable !\r\n");
                }
            }
            else
            {
                pchr_turn_off_charging_ncp1851();
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] wait gFG_booting_counter_I_FLAG==2 (%d)\r\n", gFG_booting_counter_I_FLAG);
            }
        }
	}
}

int BAT_CheckPMUStatusReg(void)
{
    if( upmu_is_chr_det() == KAL_TRUE )
    {
        BMT_status.charger_exist = TRUE;
    }
    else
    {
        BMT_status.charger_exist = FALSE;

		BMT_status.total_charging_time = 0;
		BMT_status.PRE_charging_time = 0;
		BMT_status.CC_charging_time = 0;
		BMT_status.TOPOFF_charging_time = 0;
		BMT_status.POSTFULL_charging_time = 0;

        BMT_status.bat_charging_state = CHR_PRE;

        return PMU_STATUS_FAIL;
    }

	return PMU_STATUS_OK;
}

unsigned long BAT_Get_Battery_Voltage(int polling_mode)
{
    unsigned long ret_val = 0;

    if(polling_mode == 1)
        ret_val=get_bat_sense_volt(1);
    else
        ret_val=get_bat_sense_volt(1);

    return ret_val;
}

int g_Get_I_Charging(void)
{
	kal_int32 ADC_BAT_SENSE_tmp[20]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	kal_int32 ADC_BAT_SENSE_sum=0;
	kal_int32 ADC_BAT_SENSE=0;
	kal_int32 ADC_I_SENSE_tmp[20]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	kal_int32 ADC_I_SENSE_sum=0;
	kal_int32 ADC_I_SENSE=0;
	int repeat=20;
	int i=0;
	int j=0;
	kal_int32 temp=0;
	int ICharging=0;

	for(i=0 ; i<repeat ; i++)
	{
            ADC_BAT_SENSE_tmp[i] = get_bat_sense_volt(1);
            ADC_I_SENSE_tmp[i] = get_i_sense_volt(1);

            ADC_BAT_SENSE_sum += ADC_BAT_SENSE_tmp[i];
            ADC_I_SENSE_sum += ADC_I_SENSE_tmp[i];
	}

	//sorting	BAT_SENSE
	for(i=0 ; i<repeat ; i++)
	{
		for(j=i; j<repeat ; j++)
		{
			if( ADC_BAT_SENSE_tmp[j] < ADC_BAT_SENSE_tmp[i] )
			{
				temp = ADC_BAT_SENSE_tmp[j];
				ADC_BAT_SENSE_tmp[j] = ADC_BAT_SENSE_tmp[i];
				ADC_BAT_SENSE_tmp[i] = temp;
			}
		}
	}
	if (Enable_BATDRV_LOG == 1) {
		xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[g_Get_I_Charging:BAT_SENSE]\r\n");
		for(i=0 ; i<repeat ; i++ )
		{
			xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "%d,", ADC_BAT_SENSE_tmp[i]);
		}
		xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "\r\n");
	}

	//sorting	I_SENSE
	for(i=0 ; i<repeat ; i++)
	{
		for(j=i ; j<repeat ; j++)
		{
			if( ADC_I_SENSE_tmp[j] < ADC_I_SENSE_tmp[i] )
			{
				temp = ADC_I_SENSE_tmp[j];
				ADC_I_SENSE_tmp[j] = ADC_I_SENSE_tmp[i];
				ADC_I_SENSE_tmp[i] = temp;
			}
		}
	}
	if (Enable_BATDRV_LOG == 1) {
		xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[g_Get_I_Charging:I_SENSE]\r\n");
		for(i=0 ; i<repeat ; i++ )
		{
			xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "%d,", ADC_I_SENSE_tmp[i]);
		}
		xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "\r\n");
	}

	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[0];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[1];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[18];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[19];
	ADC_BAT_SENSE = ADC_BAT_SENSE_sum / (repeat-4);

	if (Enable_BATDRV_LOG == 1) {
		xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[g_Get_I_Charging] ADC_BAT_SENSE=%d\r\n", ADC_BAT_SENSE);
	}

	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[0];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[1];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[18];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[19];
	ADC_I_SENSE = ADC_I_SENSE_sum / (repeat-4);

	if (Enable_BATDRV_LOG == 1) {
		xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[g_Get_I_Charging] ADC_I_SENSE(Before)=%d\r\n", ADC_I_SENSE);
	}

	ADC_I_SENSE += gADC_I_SENSE_offset;

	if (Enable_BATDRV_LOG == 1) {
		xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[g_Get_I_Charging] ADC_I_SENSE(After)=%d\r\n", ADC_I_SENSE);
	}

	BMT_status.ADC_BAT_SENSE = ADC_BAT_SENSE;
	BMT_status.ADC_I_SENSE = ADC_I_SENSE;

	if(ADC_I_SENSE > ADC_BAT_SENSE)
	{
    	ICharging = (ADC_I_SENSE - ADC_BAT_SENSE)*10/R_CURRENT_SENSE;
	}
	else
	{
    	ICharging = 0;
	}

	return ICharging;
}

UINT32 BattVoltToPercent(UINT16 dwVoltage)
{
    UINT32 m=0;
    UINT32 VBAT1=0,VBAT2=0;
    UINT32 bPercntResult=0,bPercnt1=0,bPercnt2=0;

	if (Enable_BATDRV_LOG == 1) {
    	xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "###### 100 <-> voltage : %d ######\r\n", Batt_VoltToPercent_Table[10].BattVolt);
	}

    if(dwVoltage<=Batt_VoltToPercent_Table[0].BattVolt)
    {
        bPercntResult = Batt_VoltToPercent_Table[0].BattPercent;
        return bPercntResult;
    }
    else if (dwVoltage>=Batt_VoltToPercent_Table[10].BattVolt)
    {
        bPercntResult = Batt_VoltToPercent_Table[10].BattPercent;
        return bPercntResult;
    }
    else
    {
        VBAT1 = Batt_VoltToPercent_Table[0].BattVolt;
        bPercnt1 = Batt_VoltToPercent_Table[0].BattPercent;
        for(m=1;m<=10;m++)
        {
            if(dwVoltage<=Batt_VoltToPercent_Table[m].BattVolt)
            {
                VBAT2 = Batt_VoltToPercent_Table[m].BattVolt;
                bPercnt2 = Batt_VoltToPercent_Table[m].BattPercent;
                break;
            }
            else
            {
                VBAT1 = Batt_VoltToPercent_Table[m].BattVolt;
                bPercnt1 = Batt_VoltToPercent_Table[m].BattPercent;
            }
        }
    }

    bPercntResult = ( ((dwVoltage-VBAT1)*bPercnt2)+((VBAT2-dwVoltage)*bPercnt1) ) / (VBAT2-VBAT1);

    return bPercntResult;

}

#if defined(MTK_JEITA_STANDARD_SUPPORT)
int do_jeita_state_machine(void)
{
    //JEITA battery temp Standard 
    if (BMT_status.temperature >= TEMP_POS_60_THRESHOLD) 
    {
        xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[BATTERY:ncp1851] Battery Over high Temperature(%d) !!\n\r", 
            TEMP_POS_60_THRESHOLD);  
        
        g_temp_status = TEMP_ABOVE_POS_60;
        
        return PMU_STATUS_FAIL; 
    }
    else if(BMT_status.temperature > TEMP_POS_45_THRESHOLD)  //control 45c to normal behavior
    {

        if((g_temp_status == TEMP_ABOVE_POS_60) && (BMT_status.temperature >= TEMP_POS_60_THRES_MINUS_X_DEGREE))
        {
            xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[BATTERY:ncp1851] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
                TEMP_POS_60_THRES_MINUS_X_DEGREE,TEMP_POS_60_THRESHOLD); 
            
            return PMU_STATUS_FAIL; 
        }
        else
        {
            xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[BATTERY:ncp1851] Battery Temperature between %d and %d !!\n\r",
                TEMP_POS_45_THRESHOLD,TEMP_POS_60_THRESHOLD); 
            
            g_temp_status = TEMP_POS_45_TO_POS_60;
            g_jeita_recharging_voltage = 3980;   
        }
    }
    else if(BMT_status.temperature >= TEMP_POS_10_THRESHOLD)
    {
        if( ((g_temp_status == TEMP_POS_45_TO_POS_60) && (BMT_status.temperature >= TEMP_POS_45_THRES_MINUS_X_DEGREE)) ||
            ((g_temp_status == TEMP_POS_0_TO_POS_10 ) && (BMT_status.temperature <= TEMP_POS_10_THRES_PLUS_X_DEGREE ))      ) 
        {
            xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[BATTERY:ncp1851] Battery Temperature not recovery to normal temperature charging mode yet!!\n\r");     
        }
        else
        {
            if(Enable_BATDRV_LOG >=1)
            {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Battery Normal Temperature between %d and %d !!\n\r",
                    TEMP_POS_10_THRESHOLD,TEMP_POS_45_THRESHOLD); 
            }
            g_temp_status = TEMP_POS_10_TO_POS_45;
            g_jeita_recharging_voltage = 4080;
        }
    }
    else if(BMT_status.temperature >= TEMP_POS_0_THRESHOLD)
    {
        if((g_temp_status == TEMP_NEG_10_TO_POS_0 || g_temp_status == TEMP_BELOW_NEG_10) && (BMT_status.temperature <= TEMP_POS_0_THRES_PLUS_X_DEGREE))
        {
			if (g_temp_status == TEMP_NEG_10_TO_POS_0) {
				xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[BATTERY:ncp1851] Battery Temperature between %d and %d !!\n\r",
					TEMP_POS_0_THRES_PLUS_X_DEGREE,TEMP_POS_10_THRESHOLD); 
			}
			if (g_temp_status == TEMP_BELOW_NEG_10) {
				xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[BATTERY:ncp1851] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
					TEMP_POS_0_THRESHOLD,TEMP_POS_0_THRES_PLUS_X_DEGREE); 
				return PMU_STATUS_FAIL; 
			}
        }    
        else
        {
            xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[BATTERY:ncp1851] Battery Temperature between %d and %d !!\n\r",
                TEMP_POS_0_THRESHOLD,TEMP_POS_10_THRESHOLD); 
            
            g_temp_status = TEMP_POS_0_TO_POS_10;
            g_jeita_recharging_voltage = 3980;
        }
    }
    else if(BMT_status.temperature >= TEMP_NEG_10_THRESHOLD)
    {
        if((g_temp_status == TEMP_BELOW_NEG_10) && (BMT_status.temperature <= TEMP_NEG_10_THRES_PLUS_X_DEGREE))
        {
            xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[BATTERY:ncp1851] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
                TEMP_NEG_10_THRESHOLD,TEMP_NEG_10_THRES_PLUS_X_DEGREE); 
            
            return PMU_STATUS_FAIL; 
        }
        else
        {
            xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[BATTERY:ncp1851] Battery Temperature between %d and %d !!\n\r",
                TEMP_NEG_10_THRESHOLD,TEMP_POS_0_THRESHOLD); 
            
            g_temp_status = TEMP_NEG_10_TO_POS_0;
            g_jeita_recharging_voltage = 3880;
        }
    }
    else
    {
        xlog_printk(ANDROID_LOG_WARN, "Power/Battery", "[BATTERY:ncp1851] Battery below low Temperature(%d) !!\n\r", 
            TEMP_NEG_10_THRESHOLD);  
        g_temp_status = TEMP_BELOW_NEG_10;
        
        return PMU_STATUS_FAIL; 
    }
}
#endif

int BAT_CheckBatteryStatus_ncp1851(void)
{
    int BAT_status = PMU_STATUS_OK;
    int i = 0;
    int bat_temperature_volt=0;

    int fg_r_value=0;
    kal_int32 fg_current_temp=0;
    kal_bool fg_current_state;
    int bat_temperature_volt_temp=0;

    /* Get Battery Information : start --------------------------------------------------------------------------*/
    if(g_chr_event != 0)
        g_chr_event = 0;

    /* Get V_BAT_SENSE */
    BMT_status.ADC_I_SENSE = get_i_sense_volt(10); //battery voltage
    BMT_status.bat_vol = BMT_status.ADC_I_SENSE;

    /* Get V_I_SENSE */
    BMT_status.ADC_BAT_SENSE = get_bat_sense_volt(10); //system voltage

    /* Get V_Charger */
    BMT_status.charger_vol = get_charger_volt(10);
    BMT_status.charger_vol = BMT_status.charger_vol / 100;

    /* Get V_BAT_Temperature */
    bat_temperature_volt = get_tbat_volt(5); 
	if(bat_temperature_volt == 0)
	{
//	    if(upmu_get_cid() == 0x1020)
//	        g_bat_temperature_pre = 22; // MT6320 E1 workaround
		BMT_status.temperature = g_bat_temperature_pre;
		if (Enable_BATDRV_LOG == 1) {
			xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY] Warning !! bat_temperature_volt == 0, restore temperature value\n\r");
		}
	}
	else
	{
#if defined(CONFIG_POWER_EXT)
        //by pass
#else
        //-----------------------------------------------------------------------------
        if( gForceADCsolution == 1 )
        {
            /*Use no gas gauge*/
        }
        else
        {
            fg_r_value = get_r_fg_value();
            fg_current_temp = fgauge_read_current();
            fg_current_temp = fg_current_temp/10;
            fg_current_state = get_gFG_Is_Charging();
            if(fg_current_state==KAL_TRUE)
            {
                bat_temperature_volt_temp = bat_temperature_volt;
                bat_temperature_volt = bat_temperature_volt - ((fg_current_temp*fg_r_value)/1000);
            }
            else
            {
                bat_temperature_volt_temp = bat_temperature_volt;
                bat_temperature_volt = bat_temperature_volt + ((fg_current_temp*fg_r_value)/1000);
            }
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[tbat_workaround] %d,%d,%d,%d,%d\n", 
                bat_temperature_volt_temp, bat_temperature_volt, fg_current_state, fg_current_temp, fg_r_value);        
        }
        //-----------------------------------------------------------------------------
#endif
        BMT_status.temperature = BattVoltToTemp(bat_temperature_volt);   
        g_bat_temperature_pre = BMT_status.temperature;
	}
	
    if((g_battery_tt_check_flag == 0) && (BMT_status.temperature < MAX_CHARGE_TEMPERATURE) && (BMT_status.temperature > MIN_CHARGE_TEMPERATURE))
    {
        g_battery_thermal_throttling_flag = 1;
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Enable battery TT (%d)\n\r", BMT_status.temperature);
        g_battery_tt_check_flag = 1;
    }

    if (Enable_BATDRV_LOG == 1) {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ADC:ncp1851] VCHR:%ld BAT_SENSE:%ld I_SENSE:%ld TBAT:%ld TBAT_vol:%ld BatTT:%d\n",
        BMT_status.charger_vol, BMT_status.ADC_BAT_SENSE, BMT_status.ADC_I_SENSE, BMT_status.temperature, bat_temperature_volt, g_battery_thermal_throttling_flag);
    }

    BMT_status.ICharging = 0; //cannot measure adaptor charing current when using power path

    g_InstatVolt = get_bat_sense_volt(20); //system voltage
    g_BatteryAverageCurrent = BMT_status.ICharging;
    g_BAT_BatterySenseVoltage = BMT_status.ADC_BAT_SENSE;
    g_BAT_ISenseVoltage = BMT_status.ADC_I_SENSE;
    g_BAT_ChargerVoltage = BMT_status.charger_vol;
    g_TempBattVoltage = BMT_status.bat_vol;
    /* Get Battery Information : end --------------------------------------------------------------------------*/

    /*Use no gas gauge*/
    if(gForceADCsolution == 1)
    {
        /* Re-calculate Battery Percentage (SOC) */
        BMT_status.SOC = BattVoltToPercent(BMT_status.bat_vol);

        /* User smooth View when discharging : start */
        if(upmu_is_chr_det() == KAL_FALSE)
        {
#if defined(MTK_JEITA_STANDARD_SUPPORT)
            if (BMT_status.bat_vol >= g_jeita_recharging_voltage)
#else        
            if (BMT_status.bat_vol >= RECHARGING_VOLTAGE) 
#endif         
            {
                BMT_status.SOC = 100;
                BMT_status.bat_full = KAL_TRUE;
            }
        }
        if (bat_volt_cp_flag == 0)
        {
            bat_volt_cp_flag = 1;
            bat_volt_check_point = BMT_status.SOC;
        }
        /* User smooth View when discharging : end */

        /**************** Averaging : START ****************/
        if (!batteryBufferFirst)
        {
            batteryBufferFirst = KAL_TRUE;

            for (i=0; i<BATTERY_AVERAGE_SIZE; i++) {
                batteryVoltageBuffer[i] = BMT_status.bat_vol;
                batteryCurrentBuffer[i] = BMT_status.ICharging;
                batterySOCBuffer[i] = BMT_status.SOC;
            }

            batteryVoltageSum = BMT_status.bat_vol * BATTERY_AVERAGE_SIZE;
            batteryCurrentSum = BMT_status.ICharging * BATTERY_AVERAGE_SIZE;
            batterySOCSum = BMT_status.SOC * BATTERY_AVERAGE_SIZE;
        }

        batteryVoltageSum -= batteryVoltageBuffer[batteryIndex];
        batteryVoltageSum += BMT_status.bat_vol;
        batteryVoltageBuffer[batteryIndex] = BMT_status.bat_vol;

        batteryCurrentSum -= batteryCurrentBuffer[batteryIndex];
        batteryCurrentSum += BMT_status.ICharging;
        batteryCurrentBuffer[batteryIndex] = BMT_status.ICharging;

        if (BMT_status.bat_full)
            BMT_status.SOC = 100;
        if (g_bat_full_user_view)
            BMT_status.SOC = 100;

        batterySOCSum -= batterySOCBuffer[batteryIndex];
        batterySOCSum += BMT_status.SOC;
        batterySOCBuffer[batteryIndex] = BMT_status.SOC;

        BMT_status.bat_vol = batteryVoltageSum / BATTERY_AVERAGE_SIZE;
        BMT_status.ICharging = batteryCurrentSum / BATTERY_AVERAGE_SIZE;
        BMT_status.SOC = batterySOCSum / BATTERY_AVERAGE_SIZE;

        batteryIndex++;
        if (batteryIndex >= BATTERY_AVERAGE_SIZE)
            batteryIndex = 0;
        /**************** Averaging : END ****************/

        if( BMT_status.SOC == 100 ) {
            BMT_status.bat_full = KAL_TRUE;
        }
    }
    /*Use gas gauge*/
    else
    {
        /* Re-calculate Battery Percentage (SOC) */
        BMT_status.SOC = FGADC_Get_BatteryCapacity_CoulombMothod();
        //BMT_status.bat_vol = FGADC_Get_FG_Voltage();

        /* Sync FG's percentage */
        if(gSyncPercentage==0)
        {
            if( (upmu_is_chr_det()==KAL_TRUE) && (!g_Battery_Fail) && (g_Charging_Over_Time==0) && (usb_is_discharging_det() == 0))
            {
                /* SOC only UP when charging */
                if ( BMT_status.SOC > bat_volt_check_point ) {
                    bat_volt_check_point = BMT_status.SOC;
                }
            }
            else
            {
                /* SOC only Done when dis-charging */
                if ( BMT_status.SOC < bat_volt_check_point ) {
                    bat_volt_check_point = BMT_status.SOC;
                }
            }
            //bat_volt_check_point = BMT_status.SOC;
        }

        /**************** Averaging : START ****************/
        if (!batteryBufferFirst)
        {
            batteryBufferFirst = KAL_TRUE;

            for (i=0; i<BATTERY_AVERAGE_SIZE; i++) {
                batteryVoltageBuffer[i] = BMT_status.bat_vol;
                batteryCurrentBuffer[i] = BMT_status.ICharging;
                batteryTempBuffer[i] = BMT_status.temperature;				
            }

            batteryVoltageSum = BMT_status.bat_vol * BATTERY_AVERAGE_SIZE;
            batteryCurrentSum = BMT_status.ICharging * BATTERY_AVERAGE_SIZE;
            batteryTempSum = BMT_status.temperature * BATTERY_AVERAGE_SIZE;			
        }

        if( (batteryCurrentSum==0) && (BMT_status.ICharging!=0) )
        {
            for (i=0; i<BATTERY_AVERAGE_SIZE; i++) {
                batteryCurrentBuffer[i] = BMT_status.ICharging;
            }
            batteryCurrentSum = BMT_status.ICharging * BATTERY_AVERAGE_SIZE;
        }

        batteryVoltageSum -= batteryVoltageBuffer[batteryIndex];
        batteryVoltageSum += BMT_status.bat_vol;
        batteryVoltageBuffer[batteryIndex] = BMT_status.bat_vol;

        batteryCurrentSum -= batteryCurrentBuffer[batteryIndex];
        batteryCurrentSum += BMT_status.ICharging;
        batteryCurrentBuffer[batteryIndex] = BMT_status.ICharging;

        batteryTempSum -= batteryTempBuffer[batteryIndex];
        batteryTempSum += BMT_status.temperature;
        batteryTempBuffer[batteryIndex] = BMT_status.temperature;   

        BMT_status.bat_vol = batteryVoltageSum / BATTERY_AVERAGE_SIZE;
        BMT_status.ICharging = batteryCurrentSum / BATTERY_AVERAGE_SIZE;
        BMT_status.temperature = batteryTempSum / BATTERY_AVERAGE_SIZE;

        batteryIndex++;
        if (batteryIndex >= BATTERY_AVERAGE_SIZE)
            batteryIndex = 0;
        /**************** Averaging : END ****************/
    }

    //workaround--------------------------------------------------
    if( (gFG_booting_counter_I_FLAG < 2) && upmu_is_chr_det() )
    {
        boot_soc_temp = BMT_status.SOC;
        boot_soc_temp_count = 1;
        printk("[Boot SOC workaround 1] boot_soc_temp=%d, BMT_status.SOC=%d, boot_soc_temp_count=%d\n",
            boot_soc_temp, BMT_status.SOC, boot_soc_temp_count);
    }
    else
    {
        if(boot_soc_temp_count > 0)
        {
            boot_soc_temp_count = 0;

            if( !(upmu_is_chr_det()) ) // charger from 1 -> 0
            {
                BMT_status.SOC = boot_soc_temp;
                g_Calibration_FG = 0;
                FGADC_Reset_SW_Parameter();
                gFG_DOD0 = 100-boot_soc_temp;
                gFG_DOD1 = gFG_DOD0;

                printk("[Boot SOC workaround 2] boot_soc_temp=%d, BMT_status.SOC=%d, boot_soc_temp_count=%d, gFG_DOD0=%d, gFG_DOD1=%d\n",
                    boot_soc_temp, BMT_status.SOC, boot_soc_temp_count, gFG_DOD0, gFG_DOD1);
            }
        }       
    }    
    //------------------------------------------------------------

    if (Enable_BATDRV_LOG == 1) {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:AVG:ncp1851] BatTemp:%d Vbat:%d VBatSen:%d SOC:%d ChrDet:%d Vchrin:%d Icharging:%d ChrType:%d USBstate:%d\r\n",
        BMT_status.temperature ,BMT_status.bat_vol, BMT_status.ADC_BAT_SENSE, BMT_status.SOC,
        upmu_is_chr_det(), BMT_status.charger_vol, BMT_status.ICharging, CHR_Type_num, g_usb_state );
    }

    if (Enable_BATDRV_LOG == 1) {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:FG:ncp1851] %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
        BMT_status.temperature ,BMT_status.bat_vol, BMT_status.ADC_BAT_SENSE, BMT_status.SOC,
        upmu_is_chr_det(), BMT_status.charger_vol, BMT_status.ICharging, CHR_Type_num,
        FGADC_Get_BatteryCapacity_CoulombMothod(), FGADC_Get_BatteryCapacity_VoltageMothod(), BATTERY_AVERAGE_SIZE );
    }

	/* Protection Check : start*/
    BAT_status = BAT_CheckPMUStatusReg();
    if(BAT_status != PMU_STATUS_OK)
    {
        return PMU_STATUS_FAIL;
    }

    if(Enable_BATDRV_LOG==1)
    {
        printk(  "[BATTERY:ncp1851] Battery Protection Check !!\n");
    }
	
    if(battery_cmd_thermal_test_mode == 1){
        BMT_status.temperature = battery_cmd_thermal_test_mode_value;
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] In thermal_test_mode, Tbat=%d\n", BMT_status.temperature);
    }	
    
#if defined(MTK_JEITA_STANDARD_SUPPORT)
    if (Enable_BATDRV_LOG == 1) {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] support JEITA, Tbat=%d\n", BMT_status.temperature);            
    }
    if( do_jeita_state_machine() == PMU_STATUS_FAIL)
    {
        printk(  "[BATTERY:ncp1851] JEITA : fail\n");
        BMT_status.bat_charging_state = CHR_ERROR;    
        return PMU_STATUS_FAIL;
    }
#else        
    #if (BAT_TEMP_PROTECT_ENABLE == 1)
    if ((BMT_status.temperature < MIN_CHARGE_TEMPERATURE) ||
        (BMT_status.temperature == ERR_CHARGE_TEMPERATURE))
    {
        printk(  "[BATTERY:ncp1851] Battery Under Temperature or NTC fail !!\n\r");                
        BMT_status.bat_charging_state = CHR_ERROR;
        return PMU_STATUS_FAIL;
    }
    #endif
    if (BMT_status.temperature >= MAX_CHARGE_TEMPERATURE)
    {
        printk(  "[BATTERY:ncp1851] Battery Over Temperature !!\n\r");     
        BMT_status.bat_charging_state = CHR_ERROR;
        return PMU_STATUS_FAIL;
    }
#endif

    //customization
    if((BMT_status.temperature <= 10) && (BMT_status.temperature >= 0))
        g_current_change_flag = 1;
    else if((g_current_change_flag == 1) && (BMT_status.temperature > 13))
        g_current_change_flag = 0;

    if( upmu_is_chr_det() == KAL_TRUE)
    {
        #if (V_CHARGER_ENABLE == 1)
        if (BMT_status.charger_vol <= V_CHARGER_MIN )
        {
            printk(  "[BATTERY:ncp1851]Charger under voltage!!\r\n");    
            BMT_status.bat_charging_state = CHR_ERROR;
            return PMU_STATUS_FAIL;
        }
        #endif
        if ( BMT_status.charger_vol >= V_CHARGER_MAX )
        {
            printk(  "[BATTERY:ncp1851]Charger over voltage!!\r\n");    
            ncp1851_read_register(7); //read 0x07-bit[7] to check if VIN > VINOV
            BMT_status.charger_protect_status = charger_OVER_VOL;
            BMT_status.bat_charging_state = CHR_ERROR;
            return PMU_STATUS_FAIL;
        }
    }
    /* Protection Check : end*/

    if( upmu_is_chr_det() == KAL_TRUE)
    {
#if defined(MTK_JEITA_STANDARD_SUPPORT)
        if((BMT_status.bat_vol < g_jeita_recharging_voltage) && (BMT_status.bat_full) && (g_HW_Charging_Done == 1) && (!g_Battery_Fail) )    
#else    
        if((BMT_status.bat_vol < RECHARGING_VOLTAGE) && (BMT_status.bat_full) && (g_HW_Charging_Done == 1) && (!g_Battery_Fail) )
#endif     
        {
            if (Enable_BATDRV_LOG == 1) {
            	xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Battery Re-charging !!\n\r");
            }
            BMT_status.bat_full = KAL_FALSE;
            BMT_status.bat_charging_state = CHR_CC;

            if(BMT_status.charger_type == STANDARD_HOST) //because tablet power consumption may exceed 450mA
            {
                g_bat_full_user_view = KAL_FALSE;
                gSyncPercentage = 0;
            }
            else
            {
                g_bat_full_user_view = KAL_TRUE;
            }

            g_HW_Charging_Done = 0;
            g_Calibration_FG = 0;

            pchr_turn_off_charging_ncp1851(); //just for toggling CHR_EN to start a new charge sequence
//          ncp1851_set_chg_en(0x0); //just for toggling CHR_EN to start a new charge sequence
            msleep(1000);

//            if (Enable_BATDRV_LOG == 1) {
//                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Battery Re-charging. Call FGADC_Reset_SW_Parameter.\n\r");
//            }
//            FGADC_Reset_SW_Parameter();
        }
    }

    //Reset error status if no error condition detected this turn
    BMT_status.charger_protect_status = 0;
    BMT_status.bat_charging_state = CHR_CC;

    return PMU_STATUS_OK;
}

PMU_STATUS BAT_BatteryStatusFailAction(void)
{
    if (Enable_BATDRV_LOG == 1) {
    	xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY] BAD Battery status... Charging Stop !!\n\r");
    }

#if defined(MTK_JEITA_STANDARD_SUPPORT)
    if((g_temp_status == TEMP_ABOVE_POS_60) ||(g_temp_status == TEMP_BELOW_NEG_10))
    {
        temp_error_recovery_chr_flag=KAL_FALSE;
    }
    	
    if((temp_error_recovery_chr_flag==KAL_FALSE) && (g_temp_status != TEMP_ABOVE_POS_60) && (g_temp_status != TEMP_BELOW_NEG_10))
    {
        temp_error_recovery_chr_flag=KAL_TRUE;
        BMT_status.bat_charging_state=CHR_PRE;
    }
#endif

    BMT_status.total_charging_time = 0;
    BMT_status.PRE_charging_time = 0;
    BMT_status.CC_charging_time = 0;
    BMT_status.TOPOFF_charging_time = 0;
    BMT_status.POSTFULL_charging_time = 0;

    /*  Disable charger */
    pchr_turn_off_charging_ncp1851();

    return PMU_STATUS_OK;
}

PMU_STATUS BAT_ChargingOTAction(void)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Charging over %d hr stop !!\n\r", MAX_CHARGING_TIME);

    //BMT_status.bat_full = KAL_TRUE;
    BMT_status.total_charging_time = 0;
    BMT_status.PRE_charging_time = 0;
    BMT_status.CC_charging_time = 0;
    BMT_status.TOPOFF_charging_time = 0;
    BMT_status.POSTFULL_charging_time = 0;

	g_HW_Charging_Done = 1;
	g_Charging_Over_Time = 1;

    /*  Disable charger*/
    pchr_turn_off_charging_ncp1851();

    return PMU_STATUS_OK;
}

extern void fg_qmax_update_for_aging(void);

PMU_STATUS BAT_BatteryFullAction(void)
{
    if (Enable_BATDRV_LOG == 1) {    
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] check Battery full !!\n\r");            
    }
    
    BMT_status.bat_full = KAL_TRUE;
    BMT_status.total_charging_time = 0;
    BMT_status.PRE_charging_time = 0;
    BMT_status.CC_charging_time = 0;
    BMT_status.TOPOFF_charging_time = 0;
    BMT_status.POSTFULL_charging_time = 0;
    
    g_HW_Charging_Done = 1;
    fg_qmax_update_for_aging();
    g_Calibration_FG = 1;
    if(gFG_can_reset_flag == 1)
    {
#if defined(MTK_JEITA_STANDARD_SUPPORT)
        if(BMT_status.bat_vol > g_jeita_recharging_voltage + 70 )  //4
#else    
        if(BMT_status.bat_vol > 4150)
#endif      
        {   
            gFG_can_reset_flag = 0;

            if (Enable_BATDRV_LOG >= 1) {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Battery real full. Call FGADC_Reset_SW_Parameter.\n");
            }
            FGADC_Reset_SW_Parameter();
        }
        else
        {
            if (Enable_BATDRV_LOG >= 1) {
                printk("[BATTERY:ncp1851] double check Battery Re-charging !!\n");                
            }
            BMT_status.bat_full = KAL_FALSE;    
            g_bat_full_user_view = KAL_TRUE;
            g_HW_Charging_Done = 0;
            if (Enable_BATDRV_LOG >= 1) {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] (%d) VBAT<4150mV. Don't FGADC_Reset_SW_Parameter.\n", BMT_status.bat_vol);
            }
        }
    }
    g_Calibration_FG = 0;

    /*  Disable charger */
    pchr_turn_off_charging_ncp1851();
    
    gSyncPercentage=1;
    
    return PMU_STATUS_OK;
}

void mt_battery_notify_check(void)
{
    g_BatteryNotifyCode = 0x0000;
    
    if(g_BN_TestMode == 0x0000)
    {
        if (Enable_BATDRV_LOG == 1) {
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[BATTERY:ncp1851] mt_battery_notify_check\n");
        }

#if defined(BATTERY_NOTIFY_CASE_0000)    
        if (Enable_BATDRV_LOG == 1) {
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[BATTERY:ncp1851] BATTERY_NOTIFY_CASE_0000\n");
        }
#endif

#if defined(BATTERY_NOTIFY_CASE_0001)
        if(BMT_status.charger_vol > V_CHARGER_MAX)
        //if(BMT_status.charger_vol > 3000) //test
        {
            g_BatteryNotifyCode |= 0x0001;
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] BMT_status.charger_vol(%d) > %d mV\n", 
                BMT_status.charger_vol, V_CHARGER_MAX);
        }
        else
        {
            g_BatteryNotifyCode &= ~(0x0001);
        }
        if (Enable_BATDRV_LOG == 1) {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] BATTERY_NOTIFY_CASE_0001 (%x)\n", 
                g_BatteryNotifyCode);
        }    
#endif

#if defined(BATTERY_NOTIFY_CASE_0002)
        if(BMT_status.temperature >= MAX_CHARGE_TEMPERATURE)
        //if(BMT_status.temperature > 20) //test
        {
            g_BatteryNotifyCode |= 0x0002;
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] bat_temp(%d) > 50'C\n", BMT_status.temperature);
        }
        else
        {
            g_BatteryNotifyCode &= ~(0x0002);
        }
        if (Enable_BATDRV_LOG == 1) {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] BATTERY_NOTIFY_CASE_0002 (%x)\n", 
                g_BatteryNotifyCode);
        }    
#endif

#if defined(BATTERY_NOTIFY_CASE_0003)
        if( (BMT_status.ICharging > 1000) &&
            (BMT_status.total_charging_time > 300)
            )
        //if(BMT_status.ICharging > 200) //test
        {
            g_BatteryNotifyCode |= 0x0004;
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] I_charging(%d) > 1000mA\n", BMT_status.ICharging);
        }
        else
        {
            g_BatteryNotifyCode &= ~(0x0004);
        }
        if (Enable_BATDRV_LOG == 1) {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] BATTERY_NOTIFY_CASE_0003 (%x)\n", 
                g_BatteryNotifyCode);
        }    
#endif

#if defined(BATTERY_NOTIFY_CASE_0004)
        if(BMT_status.bat_vol > 4350)
        //if(BMT_status.bat_vol > 3800) //test
        {
            g_BatteryNotifyCode |= 0x0008;
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] bat_vlot(%d) > 4350mV\n", BMT_status.bat_vol);
        }
        else
        {
            g_BatteryNotifyCode &= ~(0x0008);
        }
        if (Enable_BATDRV_LOG == 1) {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] BATTERY_NOTIFY_CASE_0004 (%x)\n", 
                g_BatteryNotifyCode);
        }
#endif

#if defined(BATTERY_NOTIFY_CASE_0005)
        if( (g_battery_thermal_throttling_flag==2) || (g_battery_thermal_throttling_flag==3) )
        {
            printk("[TestMode] Disable Safty Timer : no UI display\n");
        }
        else
        {
            if(BMT_status.total_charging_time >= MAX_CHARGING_TIME)
            //if(BMT_status.total_charging_time >= 60) //test
            {
                g_BatteryNotifyCode |= 0x0010;
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Charging Over Time\n");
            }
            else
            {
                g_BatteryNotifyCode &= ~(0x0010);
            }
        }
        
        if (Enable_BATDRV_LOG == 1) {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] BATTERY_NOTIFY_CASE_0005 (%x)\n", 
                g_BatteryNotifyCode);
        }
#endif

    }
    else if(g_BN_TestMode == 0x0001)
    {
        g_BatteryNotifyCode = 0x0001;
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0001\n");
    }
    else if(g_BN_TestMode == 0x0002)
    {
        g_BatteryNotifyCode = 0x0002;
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0002\n");
    }
    else if(g_BN_TestMode == 0x0003)
    {
        g_BatteryNotifyCode = 0x0004;
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0003\n");
    }
    else if(g_BN_TestMode == 0x0004)
    {
        g_BatteryNotifyCode = 0x0008;
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0004\n");
    }
    else if(g_BN_TestMode == 0x0005)
    {
        g_BatteryNotifyCode = 0x0010;
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0005\n");
    }
    else
    {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[BATTERY:ncp1851] Unknown BN_TestMode Code : %x\n", g_BN_TestMode);
    }
}

void check_battery_exist(void)
{
#if 0
    kal_uint32 baton_count = 0;

    baton_count += upmu_get_rgs_baton_undet();
    baton_count += upmu_get_rgs_baton_undet();
    baton_count += upmu_get_rgs_baton_undet();
        
    if( baton_count >= 3)
    {
        if( (g_boot_mode==META_BOOT) || (g_boot_mode==ATE_FACTORY_BOOT) )
        {
            printk("[BATTERY] boot mode = %d, bypass battery check\n", g_boot_mode);
        }
        else
        {
            printk("[BATTERY] Battery is not exist, power off NCP1851 and system (%d)\n", baton_count);
            pchr_turn_off_charging_ncp1851();
            arch_reset(0,NULL);      
        }
    }
#else
    printk("[BATTERY:ncp1851] Disable check battery exist for SMT\n");
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////
//// ncp1851
///////////////////////////////////////////////////////////////////////////////////////////
void pmic_init_for_ncp1851(void)
{
	xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[pmic_init_for_ncp1851] Start\n");

    upmu_set_rg_chrwdt_td(0x0);           // CHRWDT_TD, 4s
    upmu_set_rg_chrwdt_int_en(0);         // CHRWDT_INT_EN
    upmu_set_rg_chrwdt_en(0);             // CHRWDT_EN
    upmu_set_rg_chrwdt_wr(0);             // CHRWDT_WR

    upmu_set_rg_csdac_mode(0);      //CSDAC_MODE

    upmu_set_rg_vcdt_mode(0);       //VCDT_MODE
    upmu_set_rg_vcdt_hv_en(1);      //VCDT_HV_EN

    upmu_set_rg_bc11_bb_ctrl(1);    //BC11_BB_CTRL
    upmu_set_rg_bc11_rst(1);        //BC11_RST
    
    upmu_set_rg_csdac_mode(0);      //CSDAC_MODE
    upmu_set_rg_vbat_ov_en(0);      //VBAT_OV_EN
    upmu_set_rg_vbat_ov_vth(0x0);   //VBAT_OV_VTH, 4.3V    
//    upmu_set_rg_baton_en(1);        //BATON_EN

    //Tim, for TBAT
//    upmu_set_rg_buf_pwd_b(1);       //RG_BUF_PWD_B
    upmu_set_rg_baton_ht_en(0);     //BATON_HT_EN
    
    upmu_set_rg_ulc_det_en(0);      // RG_ULC_DET_EN=1
    upmu_set_rg_low_ich_db(1);      // RG_LOW_ICH_DB=000001'b

    upmu_set_rg_chr_en(0);           // CHR_EN
    upmu_set_rg_hwcv_en(0);          // RG_HWCV_EN

    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[pmic_init_for_ncp1851] Done\n");
}

void BAT_UpdateChargerStatus(void)
{
#if !defined(CONFIG_POWER_EXT)	
    if(gFG_booting_counter_I_FLAG == 2) {
        mt6320_ac_update(&mt6320_ac_main);
        mt6320_usb_update(&mt6320_usb_main);
    }
#endif	
}

void BAT_thread_ncp1851(void)
{
    int BAT_status = 0;
    kal_uint32 ncp1851_status=0;
    int i = 0;
    int chr_err_cnt = 0;
    int ret_val = 0;

    if(boot_check_once==1)
    {
        if( upmu_is_chr_det() == KAL_TRUE )
        {
            ret_val=get_i_sense_volt(1);
            if(ret_val > 4110)
            {
                g_bat_full_user_view = KAL_TRUE;                
            }
            printk("[BATTERY:ncp1851] ret_val=%d, g_bat_full_user_view=%d\n", ret_val, g_bat_full_user_view);
        }
        boot_check_once=0;
    }

    if (Enable_BATDRV_LOG == 1) {
#if defined(MTK_JEITA_STANDARD_SUPPORT)   
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[BAT_thread_ncp1851] LOG. %d,%d,%d, %d----------------------------\n",
        BATTERY_AVERAGE_SIZE, RECHARGING_VOLTAGE, gFG_15_vlot, mtk_jeita_support_flag);
#else
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[BAT_thread_ncp1851] LOG. %d,%d,%d----------------------------\n",
        BATTERY_AVERAGE_SIZE, RECHARGING_VOLTAGE, gFG_15_vlot);
#endif		
    }    

    if(g_battery_thermal_throttling_flag==1)
    {
        if(battery_cmd_thermal_test_mode == 1){
            BMT_status.temperature = battery_cmd_thermal_test_mode_value;
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery:ncp1851] In thermal_test_mode, Tbat=%d\n", BMT_status.temperature);
        }

#if defined(MTK_JEITA_STANDARD_SUPPORT)        
        //ignore default rule		 
#else    
        if(BMT_status.temperature >= 60)
        {
#if defined(CONFIG_POWER_EXT)
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[BATTERY:ncp1851] CONFIG_POWER_EXT, no update mt6320_battery_update_power_down.\n");
#else
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery:ncp1851] Tbat(%d)>=60, system need power down.\n", BMT_status.temperature);
            mt6320_battery_update_power_down(&mt6320_battery_main);			
            if( upmu_is_chr_det() == KAL_TRUE )
            {
                // can not power down due to charger exist, so need reset system
                arch_reset(0,NULL);
            }
            //avoid SW no feedback
            mt_power_off();
#endif
        }
#endif		
    }

    /* If charger exist, than get the charger type */
    if( upmu_is_chr_det() == KAL_TRUE )
    {
        wake_lock(&battery_suspend_lock);

        if(BMT_status.charger_type == CHARGER_UNKNOWN)
        {
            CHR_Type_num = mt_charger_type_detection();
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BAT_thread:ncp1851] CHR_Type_num = %d\r\n", CHR_Type_num);
            BMT_status.charger_type = CHR_Type_num;
            if( (CHR_Type_num==STANDARD_HOST) || (CHR_Type_num==CHARGING_HOST) )
            {
                mt_usb_connect();
            }
        }
    }
    else
    {
        wake_unlock(&battery_suspend_lock);

        BMT_status.charger_type = CHARGER_UNKNOWN;
        BMT_status.bat_full = KAL_FALSE;

        /*Use no gas gauge*/
        if( gForceADCsolution == 1 )
        {
            g_bat_full_user_view = KAL_FALSE;
        }
        /*Use gas gauge*/
        else
        {
            if(bat_volt_check_point != 100)
            {
                g_bat_full_user_view = KAL_FALSE;
                if (Enable_BATDRV_LOG == 1)
                {
                    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[Battery_Only:ncp1851] Set g_bat_full_user_view=KAL_FALSE\r\n");
                }
            }
        }

        g_usb_state = USB_UNCONFIGURED;
        g_HW_Charging_Done = 0;
        g_Charging_Over_Time = 0;
        g_Calibration_FG = 0;
        mt_usb_disconnect();
        for (i=0; i<BATTERY_AVERAGE_SIZE; i++) {
            batteryCurrentBuffer[i] = 0;
        }
        batteryCurrentSum = 0;
    }

    /* Check Battery Status */
    BAT_status = BAT_CheckBatteryStatus_ncp1851();

    if(BMT_status.bat_charging_state == CHR_ERROR)
    {
        chr_err_cnt = 60; //10mins debounce
    }
    else if(chr_err_cnt > 0)
    {
        chr_err_cnt --;
    }
	
    if( BAT_status == PMU_STATUS_FAIL )
        g_Battery_Fail = KAL_TRUE;
    else if( chr_err_cnt > 0 )
        g_Battery_Fail = KAL_TRUE;
    else
        g_Battery_Fail = KAL_FALSE;

#if defined(MTK_JEITA_STANDARD_SUPPORT)
    if(g_last_temp_status ==g_temp_status){
        battery_temprange_change_flag=0;
    }else{
        battery_temprange_change_flag=1;
    }  

    if(battery_temprange_change_flag==1)	//if temperature range change, then reset cv threshold	Tim 20120803 report
    {					 
        if(g_temp_status == TEMP_NEG_10_TO_POS_0)
        {
            ncp1851_set_ctrl_vbat(0x1C); //VCHG = 4.0V
        }
        else if(g_temp_status == TEMP_POS_10_TO_POS_45)
        {
            if((ncp1851_status == 0x8) || (ncp1851_status == 0x9) || (ncp1851_status == 0xA)) //WEAK WAIT, WEAK SAFE, WEAK CHARGE
                ncp1851_set_ctrl_vbat(0x1C); //VCHG = 4.0V
            else if(ncp1851_status == 0x4)
                ncp1851_set_ctrl_vbat(0x28); //VCHG = 4.3V to decrease charge time
            else
                ncp1851_set_ctrl_vbat(0x24); //VCHG = 4.2V			
        }
        else if((g_temp_status == TEMP_POS_0_TO_POS_10)||(g_temp_status == TEMP_POS_45_TO_POS_60))
        {	  
            ncp1851_set_ctrl_vbat(0x20); //VCHG = 4.1V
        }
        else{
            ncp1851_set_ctrl_vbat(0x24); //VCHG = 4.2V			
        }
    }
#else
    //ignore
#endif

    if(battery_cmd_thermal_test_mode == 1){
        BMT_status.temperature = battery_cmd_thermal_test_mode_value;
	    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] In thermal_test_mode 3, Tbat=%d\n", BMT_status.temperature);
    }
    
    /* Battery Notify Check */
    mt_battery_notify_check();

#if defined(CONFIG_POWER_EXT)
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] CONFIG_POWER_EXT, no update Android.\n");
#else
    if(gFG_booting_counter_I_FLAG == 2)
    {
         /* AC/USB/Battery information update for Android */
         mt6320_ac_update(&mt6320_ac_main);
         mt6320_usb_update(&mt6320_usb_main);
         mt6320_battery_update(&mt6320_battery_main);
    }
    else if(gFG_booting_counter_I_FLAG == 1)
    {
        /*Use no gas gauge*/
        if( gForceADCsolution == 1 )
        {
            //do nothing
        }
        else
        {
            mt6320_battery_main.BAT_CAPACITY = 50;
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] gFG_booting_counter_I_FLAG is 1, SOC=50.\n");
        }
    }	
    else
    {
         xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] gFG_booting_counter_I_FLAG!=2 (%d)\r\n", gFG_booting_counter_I_FLAG);
    }
#endif

    /* No Charger */
    if(BAT_status == PMU_STATUS_FAIL || g_Battery_Fail)
    {
        gFG_can_reset_flag = 1;

        BMT_status.total_charging_time = 0;
        BMT_status.PRE_charging_time = 0;
        BMT_status.CC_charging_time = 0;
        BMT_status.TOPOFF_charging_time = 0;
        BMT_status.POSTFULL_charging_time = 0;

        pchr_turn_off_charging_ncp1851();

        if (Enable_BATDRV_LOG == 1)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] PMU_STATUS_FAIL (chr_err_cnt = %d)\n", chr_err_cnt);
        }
    }
    /* Battery Full */
    /* HW charging done, real stop charging */
    else if (g_HW_Charging_Done == 1)
    {
        if (Enable_BATDRV_LOG == 1)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Battery real full. \n");
        }
        BMT_status.bat_full = KAL_TRUE;
        BMT_status.total_charging_time = 0;
        BMT_status.PRE_charging_time = 0;
        BMT_status.CC_charging_time = 0;
        BMT_status.TOPOFF_charging_time = 0;
        BMT_status.POSTFULL_charging_time = 0;
        g_HW_Charging_Done = 1;
        fg_qmax_update_for_aging();
        g_Calibration_FG = 1;
#ifdef NCP1851_PWR_PATH
        pchr_turn_on_charging_ncp1851();//do not turn off charging because NCP1851 has a power path management feature
#else
        pchr_turn_off_charging_ncp1851();
#endif

        if(gFG_can_reset_flag == 1)
        {
            gFG_can_reset_flag = 0;
            if (Enable_BATDRV_LOG == 1)
            {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Battery real full. Call FGADC_Reset_SW_Parameter.\n");
            }
            FGADC_Reset_SW_Parameter();
        }
        g_Calibration_FG = 0;
        gSyncPercentage = 1;
    }

    /* Charging Overtime, can not charging (not for total charging timer) */
    else if (g_Charging_Over_Time == 1)
    {
        if (Enable_BATDRV_LOG == 1)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Charging Over Time. \n");
        }
        pchr_turn_off_charging_ncp1851();

        if(gFG_can_reset_flag == 1)
        {
            gFG_can_reset_flag = 0;
        }
    }

    /* Battery Not Full and Charger exist : Do Charging */
    else
    {
        gFG_can_reset_flag = 1;

        if (Enable_BATDRV_LOG == 1)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] state=%ld, \n", BMT_status.bat_charging_state);
        }

        /* Charging OT */
        if(BMT_status.total_charging_time >= MAX_CHARGING_TIME)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Charging over %d hr stop !!\n\r", MAX_CHARGING_TIME);        
            BMT_status.total_charging_time = 0;
            BMT_status.PRE_charging_time = 0;
            BMT_status.CC_charging_time = 0;
            BMT_status.TOPOFF_charging_time = 0;
            BMT_status.POSTFULL_charging_time = 0;
            g_HW_Charging_Done = 1;
            g_Charging_Over_Time = 1;
            pchr_turn_off_charging_ncp1851();
            return;
        }

        ncp1851_status = ncp1851_get_chip_status();

        /* check battery full */
        if((BMT_status.bat_vol > RECHARGING_VOLTAGE) && ((ncp1851_status == 0x6) || (ncp1851_status == 0x7))) //TODO: monitor charge done, check if need to monitor DPP
        {
            BMT_status.bat_charging_state = CHR_BATFULL;
            BMT_status.bat_full = KAL_TRUE;
            BMT_status.total_charging_time = 0;
            BMT_status.PRE_charging_time = 0;
            BMT_status.CC_charging_time = 0;
            BMT_status.TOPOFF_charging_time = 0;
            BMT_status.POSTFULL_charging_time = 0;
            g_HW_Charging_Done = 1;
            fg_qmax_update_for_aging();
            g_Calibration_FG = 1;
#ifdef NCP1851_PWR_PATH
            pchr_turn_on_charging_ncp1851(); //do not turn off charging because NCP1851 has a power path management feature
#else
            pchr_turn_off_charging_ncp1851();
#endif

            if(gFG_can_reset_flag == 1)
            {
                gFG_can_reset_flag = 0;

                if (Enable_BATDRV_LOG == 1) {
                    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Battery real full. Call FGADC_Reset_SW_Parameter.\n");
                }
                FGADC_Reset_SW_Parameter();
            }
            g_Calibration_FG = 0;

            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Battery real full and disable charging (%d) \n", ncp1851_status);
            gSyncPercentage = 1;
            return;
        }
        else if((ncp1851_status == 0x6) || (ncp1851_status == 0x7)) //false alarm, reset status
        {
            pchr_turn_off_charging_ncp1851();
            msleep(100);
            if (Enable_BATDRV_LOG == 1) {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Ncp1851_status is (%d), but battery voltage not over recharge voltage\n", ncp1851_status);
            }
        }

        /* Charging flow begin */
        BMT_status.total_charging_time += BAT_TASK_PERIOD;
        BMT_status.bat_charging_state = CHR_CC;
        pchr_turn_on_charging_ncp1851();
        if (Enable_BATDRV_LOG == 1)
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Total charging timer=%d, ncp1851_status=%d \n\r",
            BMT_status.total_charging_time, ncp1851_status);
        }
    }

#if defined(MTK_JEITA_STANDARD_SUPPORT)
    g_last_temp_status = g_temp_status;
#endif

    if (Enable_BATDRV_LOG == 1)
    {
        ncp1851_dump_register();
    }

    if (Enable_BATDRV_LOG == 1)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BAT_thread_ncp1851] End ....\n");
    }
}

int g_FG_init = 0;
int g_bat_init_flag=0;

void PrechargeCheckStatus(void)
{
    kal_int32 bat_vol, sys_vol;
    int bat_temp_vol, charger_vol;
    int bat_temp = 0;
    int bat_vol_chk = 0;
    kal_uint32 vfet_status, ncp1851_status;
    struct power_supply *bat_psy;
    struct mt6320_battery_data *bat_data;
    int chr_err_cnt = 0;

    if(g_FG_init == 1)
        return;
    
    while(1)
    {
        if((g_bat_init_flag == 1) && (g_pmic_init_for_ncp1851 == 1)) //make sure mt6320_battery_probe register finished
            break;
        else
            msleep(1000);
    }

    if( (get_boot_mode()==META_BOOT) || (get_boot_mode()==ADVMETA_BOOT) )
    {
        pchr_turn_off_charging_ncp1851();
        return;
    }
    
    ncp1851_dump_register();

    if( get_charger_detect_status() == KAL_TRUE ) //do not check USB device check at precharge state
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Boot up during charging!\n");
        g_boot_charging = 1;
        ncp1851_set_ctrl_vfet(0x5); // VFET = 3.6V
        ncp1851_set_chg_en(0x1); // charger enable, in case adaptor plug in during booting period        
        msleep(100);
        vfet_status = ncp1851_get_vfet_ok();
        ncp1851_status = ncp1851_get_chip_status();
        bat_vol = get_i_sense_volt(10);
        if(vfet_status == 1)
        {
            g_ocv_lookup_done = 0;
            pchr_turn_off_charging_ncp1851();
            bat_vol = get_i_sense_volt(10);
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Powerpath init battery voltage > 3600, battery voltage during system discharging = %d\n", bat_vol);
            return;
        }
        else if(bat_vol >= 3700) //Error handling when vfet_status is not reliable
        {
            g_ocv_lookup_done = 0;
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Powerpath init, (vfet_status = %d) (ncp1851_status = %d)\n", vfet_status, ncp1851_status);
            pchr_turn_off_charging_ncp1851();
            bat_vol = get_i_sense_volt(10);
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Powerpath init battery voltage > 3600, battery voltage during system discharging = %d\n", bat_vol);
            return;
        }
        else
        {
            fgauge_precharge_init();
            gFG_capacity = 0;
            bat_volt_check_point = 0;
            g_ocv_lookup_done = 1;
        }
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Powerpath init battery voltage < 3600, battery voltage  = %d\n", bat_vol);
    }
    else
    {
        return;
    }

    while(1)
    {
        if( get_charger_detect_status() == KAL_TRUE ) //do not check USB device check at precharge state
        {
            wake_lock(&battery_suspend_lock);

            if(BMT_status.charger_type == CHARGER_UNKNOWN)
            {
                CHR_Type_num = mt_charger_type_detection();
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] CHR_Type_num = %d\r\n", CHR_Type_num);
                BMT_status.charger_type = CHR_Type_num;
                if( (CHR_Type_num==STANDARD_HOST) || (CHR_Type_num==CHARGING_HOST) )
                {
                    mt_usb_connect();
                }
            }

            bat_vol = fgauge_precharge_compensated_voltage(7);

            ncp1851_set_otg_en(0x0); //disable boost mode at precharge state
            ncp1851_set_ntc_en(0x0);
            ncp1851_set_tj_warn_opt(0x1);
            ncp1851_set_jeita_opt(0x1);
            ncp1851_set_int_mask(0x0); //disable all interrupt
            ncp1851_set_tchg_rst(0x1); //reset charge timer
#ifdef NCP1851_PWR_PATH
            ncp1851_set_pwr_path(0x1);
#else
            ncp1851_set_pwr_path(0x0);
#endif
            ncp1851_status = ncp1851_get_chip_status();

            if((ncp1851_status == 0x8) || (ncp1851_status == 0x9) || (ncp1851_status == 0xA)) //WEAK WAIT, WEAK SAFE, WEAK CHARGE
                ncp1851_set_ctrl_vbat(0x1C); //VCHG = 4.0V            
            else
                ncp1851_set_ctrl_vbat(0x24); //VCHG = 4.2V

            if((bat_vol <= 3400) && (bat_vol_chk == 0) && ((BMT_status.charger_type != STANDARD_HOST) && (BMT_status.charger_type != CHARGER_UNKNOWN)))
            {
                ncp1851_set_iweak(0x3); //weak charge current = 300mA
            }
            else
            {
                bat_vol_chk = 1;
                ncp1851_set_iweak(0x1); //weak charge current = 100mA
            }

            ncp1851_set_ctrl_vfet(0x5); // VFET = 3.6V

            select_charging_curret_ncp1851();

            bat_temp_vol = get_tbat_volt(5);
            bat_temp = BattVoltToTemp(bat_temp_vol);

            if((g_battery_tt_check_flag == 0) && (bat_temp < 60) && (bat_temp > (-20)))
            {
                g_battery_thermal_throttling_flag = 1;
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Powerpath precharge enable battery TT (%d)\n\r", bat_temp);
                g_battery_tt_check_flag = 1;
            }

            charger_vol = get_charger_volt(10);
            charger_vol = charger_vol / 100;
            BMT_status.charger_vol = charger_vol; //for Eng mode display

            BMT_status.bat_charging_state = CHR_PRE;

            if(battery_cmd_thermal_test_mode == 1){
                BMT_status.temperature = battery_cmd_thermal_test_mode_value;
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] In thermal_test_mode, Tbat=%d\n", BMT_status.temperature);
            }			
#if (BAT_TEMP_PROTECT_ENABLE == 1)
            if ((bat_temp <= MIN_CHARGE_TEMPERATURE) ||
            (bat_temp == ERR_CHARGE_TEMPERATURE))
            {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery",	"[BATTERY:ncp1851] Powerpath precharge battery Under Temperature or NTC fail !!\n\r");
                BMT_status.bat_charging_state = CHR_ERROR;
            }
#endif
            if (bat_temp >= MAX_CHARGE_TEMPERATURE)
            {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery",	"[BATTERY:ncp1851] Powerpath precharge battery Over Temperature !!\n\r");
                BMT_status.bat_charging_state = CHR_ERROR;
            }
			
#if (V_CHARGER_ENABLE == 1)
            if (charger_vol <= V_CHARGER_MIN )
            {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery",	"[BATTERY:ncp1851] Powerpath precharge charger under voltage!!\r\n");
                BMT_status.bat_charging_state = CHR_ERROR;
            }
#endif
            if ( charger_vol >= V_CHARGER_MAX )
            {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery",	"[BATTERY:ncp1851] Powerpath precharge charger over voltage !!\r\n");
                BMT_status.bat_charging_state = CHR_ERROR;
            }
			
            if(BMT_status.bat_charging_state == CHR_ERROR)
            {
                chr_err_cnt = 120; // 120 * 5 = 600secs = 10 mins debounce
            }
            else if(chr_err_cnt > 0)
            {					 
                chr_err_cnt--;
            }

            if(( g_temp_CC_value == Cust_CC_0MA) || (chr_err_cnt > 0))
            {
                ncp1851_set_chg_en(0x0); // charger disable
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery",	"[BATTERY:ncp1851] Powerpath precharge chr_err_cnt = %d\r\n", chr_err_cnt);
            }
            else
            {
                ncp1851_set_chg_en(0x1); // charger enable
                //Set SPM = 1, for lenovo platform, GPIO_CHR_SPM_PIN
                mt_set_gpio_mode(GPIO_CHR_SPM_PIN, GPIO_MODE_00);
                mt_set_gpio_dir(GPIO_CHR_SPM_PIN, GPIO_DIR_OUT);
                mt_set_gpio_out(GPIO_CHR_SPM_PIN, GPIO_OUT_ONE);
                if (Enable_BATDRV_LOG == 1) {
                    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Power path precharging !\r\n");
                }
            }

            if((bat_vol >= SYSTEM_OFF_VOLTAGE + 100) || (vfet_status == 1))
            {
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Powerpath precharge finish, battery voltage = %d\n", bat_vol);
                BMT_status.bat_charging_state = CHR_CC;
                break;
            }
            else
            {
#if !defined(CONFIG_POWER_EXT)            
                //update to android UI
                mt6320_ac_update(&mt6320_ac_main);
                mt6320_usb_update(&mt6320_usb_main);
                bat_data = &mt6320_battery_main;
                bat_psy = &bat_data->psy;
                bat_data->BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
                bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
                bat_data->BAT_batt_vol = get_i_sense_volt(10);
                bat_data->BAT_batt_temp = 10 * bat_temp;
                bat_data->BAT_PRESENT = 1;
                bat_data->BAT_CAPACITY = bat_volt_check_point;
                bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_CHARGING;

                /* Update for EM */
                bat_data->BAT_TemperatureR=10000;
                bat_data->BAT_TempBattVoltage=bat_vol;
                bat_data->BAT_InstatVolt=bat_vol;
                bat_data->BAT_BatteryAverageCurrent=0;
                bat_data->BAT_BatterySenseVoltage=bat_vol;
                bat_data->BAT_ISenseVoltage=bat_vol;
                bat_data->BAT_ChargerVoltage=charger_vol;

                power_supply_changed(bat_psy);

                sys_vol = get_bat_sense_volt(10);
                xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Powerpath precharge continue, battery voltage (%d), system voltage (%d), charger volt (%d)\n", bat_vol, sys_vol, charger_vol);

                mt_battery_notify_check();
#endif
                msleep(5000);
            }
        }
        else
        {
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851]VCDT voltage = %d\n", get_charger_volt(10));        
            wake_unlock(&battery_suspend_lock);
            pchr_turn_off_charging_ncp1851();
            BMT_status.bat_charging_state = CHR_PRE;

            BMT_status.charger_type = CHARGER_UNKNOWN;

            g_usb_state = USB_UNCONFIGURED;
            g_HW_Charging_Done = 0;
            g_Charging_Over_Time = 0;
            g_Calibration_FG = 0;
            mt_usb_disconnect();
			
#if !defined(CONFIG_POWER_EXT)            			
            //wait system power off if adaptor plug out during precharge state
            mt6320_ac_update(&mt6320_ac_main);
            mt6320_usb_update(&mt6320_usb_main);

            charger_vol = get_charger_volt(10);
            charger_vol = charger_vol / 100;
            BMT_status.charger_vol = charger_vol; //for Eng mode display

            bat_data = &mt6320_battery_main;
            bat_psy = &bat_data->psy;
            bat_data->BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
            bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
            bat_data->BAT_batt_vol = get_i_sense_volt(10);
            bat_temp_vol = get_tbat_volt(5);
            bat_data->BAT_batt_temp = 10 * bat_temp;
            bat_data->BAT_PRESENT = 1;
            bat_data->BAT_CAPACITY = 0;
            bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_NOT_CHARGING;

            /* Update for EM */
            bat_data->BAT_TemperatureR=10000;
            bat_data->BAT_TempBattVoltage=bat_vol;
            bat_data->BAT_InstatVolt=bat_vol;
            bat_data->BAT_BatteryAverageCurrent=0;
            bat_data->BAT_BatterySenseVoltage=bat_vol;
            bat_data->BAT_ISenseVoltage=bat_vol;
            bat_data->BAT_ChargerVoltage=charger_vol;

            power_supply_changed(bat_psy);
#endif
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[BATTERY:ncp1851] Powerpath precharge stopped due to charger plug-out, wait for system shutdown\n");
            msleep(5000);
        }
    }

    fgauge_precharge_uninit();
    return;
}

///////////////////////////////////////////////////////////////////////////////////////////
//// Internal API
///////////////////////////////////////////////////////////////////////////////////////////
int bat_thread_kthread(void *x)
{
    mutex_lock(&bat_mutex);
    pmic_init_for_ncp1851();
    PrechargeCheckStatus();
    mutex_unlock(&bat_mutex);	

    /* Run on a process content */  
    while (1) {               
        
        if(g_battery_flag_resume==0)
        {
            mutex_lock(&bat_mutex);
            
#if defined(CONFIG_POWER_EXT)
            BAT_thread_ncp1851();
#else
            if(g_FG_init == 0)
            {
                g_FG_init=1;
                fgauge_initialization();
                FGADC_thread_kthread();
                //sync FG timer
                FGADC_thread_kthread();				
            }
            else
            {            
                // if plug-in/out USB, bypass once
                if(g_chr_event==0)
                {            
                    FGADC_thread_kthread();
                }
                
                BAT_thread_ncp1851();                      
            }
#endif

            mutex_unlock(&bat_mutex);
        }
        else
        {
            g_battery_flag_resume=0;
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[bat_thread_kthread] g_battery_flag_resume=%d\r\n", g_battery_flag_resume);
        }
        
        if (Enable_BATDRV_LOG == 1) {
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "******** MT6320 battery : bat_thread_kthread : 1 ********\n" );
        }

        wait_event(bat_thread_wq, bat_thread_timeout);

        if (Enable_BATDRV_LOG == 1) {
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "******** MT6320 battery : bat_thread_kthread : 2 ********\n" );
        }
        
        bat_thread_timeout=0;

        if( g_wake_up_bat==1 )
        {
            g_wake_up_bat=0;
            g_Calibration_FG = 0;
            FGADC_Reset_SW_Parameter();
            
            if (Enable_BATDRV_LOG == 1) {
                xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[BATTERY] Call FGADC_Reset_SW_Parameter.\r\n");
            }
        }
        
    }

    return 0;
}

UINT32 bat_thread_timeout_sum=0;

void bat_thread_wakeup(void)
{
    if (Enable_BATDRV_LOG == 1) {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "******** MT6320 battery : bat_thread_wakeup : 1 ********\n" );
    }
    
    bat_thread_timeout = 1;
    wake_up(&bat_thread_wq);    
    
    if (Enable_BATDRV_LOG == 1) {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "******** MT6320 battery : bat_thread_wakeup : 2 ********\n" );
    }    
}

///////////////////////////////////////////////////////////////////////////////////////////
//// fop API
///////////////////////////////////////////////////////////////////////////////////////////
static long adc_cali_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int *user_data_addr;
    int *naram_data_addr;
    int i = 0;
    int ret = 0;
    int bat_thread_ctrl[1];

    mutex_lock(&bat_mutex);

    switch(cmd)
    {
        case TEST_ADC_CALI_PRINT :
            g_ADC_Cali = KAL_FALSE;
            break;

        case SET_ADC_CALI_Slop:
            naram_data_addr = (int *)arg;
            ret = copy_from_user(adc_cali_slop, naram_data_addr, 36);
            g_ADC_Cali = KAL_FALSE; /* enable calibration after setting ADC_CALI_Cal */
            /* Protection */
            for (i=0;i<14;i++)
            {
                if ( (*(adc_cali_slop+i) == 0) || (*(adc_cali_slop+i) == 1) ) {
                    *(adc_cali_slop+i) = 1000;
                }
            }
            for (i=0;i<14;i++) xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "adc_cali_slop[%d] = %d\n",i , *(adc_cali_slop+i));
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "**** unlocked_ioctl : SET_ADC_CALI_Slop Done!\n");
            break;

        case SET_ADC_CALI_Offset:
            naram_data_addr = (int *)arg;
            ret = copy_from_user(adc_cali_offset, naram_data_addr, 36);
            g_ADC_Cali = KAL_FALSE; /* enable calibration after setting ADC_CALI_Cal */
            for (i=0;i<14;i++) xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "adc_cali_offset[%d] = %d\n",i , *(adc_cali_offset+i));
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "**** unlocked_ioctl : SET_ADC_CALI_Offset Done!\n");
            break;

        case SET_ADC_CALI_Cal :
            naram_data_addr = (int *)arg;
            ret = copy_from_user(adc_cali_cal, naram_data_addr, 4);
            g_ADC_Cali = KAL_TRUE;
            if ( adc_cali_cal[0] == 1 ) {
                g_ADC_Cali = KAL_TRUE;
            } else {
                g_ADC_Cali = KAL_FALSE;
            }
            for (i=0;i<1;i++) xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "adc_cali_cal[%d] = %d\n",i , *(adc_cali_cal+i));
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "**** unlocked_ioctl : SET_ADC_CALI_Cal Done!\n");
            break;

        case ADC_CHANNEL_READ:
            //g_ADC_Cali = KAL_FALSE; /* 20100508 Infinity */
            user_data_addr = (int *)arg;
            ret = copy_from_user(adc_in_data, user_data_addr, 8); /* 2*int = 2*4 */

            if( adc_in_data[0] == 0 ) // I_SENSE
            {
                adc_out_data[0] = get_i_sense_volt(adc_in_data[1]) * adc_in_data[1];
            }
            else if( adc_in_data[0] == 1 ) // BAT_SENSE
            {
                adc_out_data[0] = get_bat_sense_volt(adc_in_data[1]) * adc_in_data[1];
            }
            else if( adc_in_data[0] == 3 ) // V_Charger
            {
                adc_out_data[0] = get_charger_volt(adc_in_data[1]) * adc_in_data[1];
                adc_out_data[0] = adc_out_data[0] / 100;
            }    
            else if( adc_in_data[0] == 30 ) // V_Bat_temp magic number
            {
                //adc_out_data[0] = PMIC_IMM_GetOneChannelValue(AUXADC_TEMPERATURE_CHANNEL,adc_in_data[1]) * adc_in_data[1];
                adc_out_data[0] = BMT_status.temperature;                
            }
            else if( adc_in_data[0] == 66 ) 
            {
                adc_out_data[0] = (gFG_current)/10;
                
                if (gFG_Is_Charging == KAL_TRUE) 
                {                    
                    adc_out_data[0] = 0 - adc_out_data[0]; //charging
                }                                
            }
            else
            {
                adc_out_data[0] = PMIC_IMM_GetOneChannelValue(adc_in_data[0],adc_in_data[1],1) * adc_in_data[1];
            }

            if (adc_out_data[0]<0)
                adc_out_data[1]=1; /* failed */
            else
                adc_out_data[1]=0; /* success */

            if( adc_in_data[0] == 30 )
                adc_out_data[1]=0; /* success */

            if( adc_in_data[0] == 66 )
                adc_out_data[1]=0; /* success */

            ret = copy_to_user(user_data_addr, adc_out_data, 8);
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "**** unlocked_ioctl : Channel %d * %d times = %d\n", adc_in_data[0], adc_in_data[1], adc_out_data[0]);
            break;

        case BAT_STATUS_READ:
            user_data_addr = (int *)arg;
            ret = copy_from_user(battery_in_data, user_data_addr, 4);
            /* [0] is_CAL */
            if (g_ADC_Cali) {
                battery_out_data[0] = 1;
            } else {
                battery_out_data[0] = 0;
            }
            ret = copy_to_user(user_data_addr, battery_out_data, 4);
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "**** unlocked_ioctl : CAL:%d\n", battery_out_data[0]);
            break;

        case Set_Charger_Current: /* For Factory Mode*/
            user_data_addr = (int *)arg;
            ret = copy_from_user(charging_level_data, user_data_addr, 4);
            g_ftm_battery_flag = KAL_TRUE;
            if( charging_level_data[0] == 0 ) { 			charging_level_data[0] = 100;
            } else if ( charging_level_data[0] == 1  ) {	charging_level_data[0] = 400;
            } else if ( charging_level_data[0] == 2  ) {	charging_level_data[0] = 500;
            } else if ( charging_level_data[0] == 3  ) {	charging_level_data[0] = 600;
            } else if ( charging_level_data[0] == 4  ) {	charging_level_data[0] = 700;
            } else if ( charging_level_data[0] == 5  ) {	charging_level_data[0] = 800;
            } else if ( charging_level_data[0] == 6  ) {	charging_level_data[0] = 900;
            } else if ( charging_level_data[0] == 7  ) {	charging_level_data[0] = 1000;
            } else if ( charging_level_data[0] == 8  ) {	charging_level_data[0] = 1100;
            } else if ( charging_level_data[0] == 9  ) {	charging_level_data[0] = 1200;
            } else if ( charging_level_data[0] == 10 ) {	charging_level_data[0] = 1300;
            } else if ( charging_level_data[0] == 11 ) {	charging_level_data[0] = 1400;
            } else if ( charging_level_data[0] == 12 ) {	charging_level_data[0] = 1500;
            } else if ( charging_level_data[0] == 13 ) {	charging_level_data[0] = 1600;
            } else {
                charging_level_data[0] = Cust_CC_500MA;
            }
            wake_up_bat();
            xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "**** unlocked_ioctl : set_Charger_Current:%d\n", charging_level_data[0]);
            break;

            case AVG_BAT_SEN_READ:
                user_data_addr = (int *)arg;
                batsen_out_data[0] = BMT_status.bat_vol;
                ret = copy_to_user(user_data_addr, batsen_out_data, 4);
                xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "**** unlocked_ioctl : CAL:%d\n", batsen_out_data[0]);
            break;

            case FGADC_CURRENT_READ:
                user_data_addr = (int *)arg;
                if(gFG_Is_Charging == KAL_FALSE)
                    fgadc_out_data[0] = 0;
                else
                    fgadc_out_data[0] = gFG_current / 10;
                ret = copy_to_user(user_data_addr, fgadc_out_data, 4);
                xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "**** unlocked_ioctl : CAL:%d\n", fgadc_out_data[0]);
            break;

            case BAT_THREAD_CTRL:
                user_data_addr = (int *)arg;
                ret = copy_from_user(bat_thread_ctrl, user_data_addr, 4);
                bat_thread_control(bat_thread_ctrl[0]);
                xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "**** unlocked_ioctl : bat_thread_ctrl:%d\n", bat_thread_ctrl[0]);
            break;

        default:
            g_ADC_Cali = KAL_FALSE;
            break;
    }

    mutex_unlock(&bat_mutex);

    return 0;
}

static int adc_cali_open(struct inode *inode, struct file *file)
{
   return 0;
}

static int adc_cali_release(struct inode *inode, struct file *file)
{
    return 0;
}

static struct file_operations adc_cali_fops = {
    .owner        = THIS_MODULE,
    .unlocked_ioctl	= adc_cali_ioctl,
    .open        = adc_cali_open,
    .release    = adc_cali_release,
};

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Charger_Voltage
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Charger_Voltage(struct device *dev,struct device_attribute *attr, char *buf)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] show_ADC_Charger_Voltage : %d\n", BMT_status.charger_vol);
    return sprintf(buf, "%d\n", BMT_status.charger_vol);
}
static ssize_t store_ADC_Charger_Voltage(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");
    return size;
}
static DEVICE_ATTR(ADC_Charger_Voltage, 0664, show_ADC_Charger_Voltage, store_ADC_Charger_Voltage);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_0_Slope
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_0_Slope(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_slop+0));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_0_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_0_Slope(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_0_Slope, 0664, show_ADC_Channel_0_Slope, store_ADC_Channel_0_Slope);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_1_Slope
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_1_Slope(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_slop+1));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_1_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_1_Slope(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_1_Slope, 0664, show_ADC_Channel_1_Slope, store_ADC_Channel_1_Slope);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_2_Slope
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_2_Slope(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_slop+2));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_2_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_2_Slope(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_2_Slope, 0664, show_ADC_Channel_2_Slope, store_ADC_Channel_2_Slope);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_3_Slope
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_3_Slope(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_slop+3));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_3_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_3_Slope(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_3_Slope, 0664, show_ADC_Channel_3_Slope, store_ADC_Channel_3_Slope);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_4_Slope
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_4_Slope(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_slop+4));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_4_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_4_Slope(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_4_Slope, 0664, show_ADC_Channel_4_Slope, store_ADC_Channel_4_Slope);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_5_Slope
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_5_Slope(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_slop+5));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_5_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_5_Slope(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_5_Slope, 0664, show_ADC_Channel_5_Slope, store_ADC_Channel_5_Slope);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_6_Slope
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_6_Slope(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_slop+6));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_6_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_6_Slope(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_6_Slope, 0664, show_ADC_Channel_6_Slope, store_ADC_Channel_6_Slope);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_7_Slope
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_7_Slope(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_slop+7));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_7_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_7_Slope(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_7_Slope, 0664, show_ADC_Channel_7_Slope, store_ADC_Channel_7_Slope);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_8_Slope
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_8_Slope(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_slop+8));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_8_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_8_Slope(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_8_Slope, 0664, show_ADC_Channel_8_Slope, store_ADC_Channel_8_Slope);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_9_Slope
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_9_Slope(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_slop+9));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_9_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_9_Slope(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_9_Slope, 0664, show_ADC_Channel_9_Slope, store_ADC_Channel_9_Slope);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_10_Slope
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_10_Slope(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_slop+10));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_10_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_10_Slope(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_10_Slope, 0664, show_ADC_Channel_10_Slope, store_ADC_Channel_10_Slope);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_11_Slope
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_11_Slope(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_slop+11));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_11_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_11_Slope(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_11_Slope, 0664, show_ADC_Channel_11_Slope, store_ADC_Channel_11_Slope);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_12_Slope
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_12_Slope(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_slop+12));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_12_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_12_Slope(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_12_Slope, 0664, show_ADC_Channel_12_Slope, store_ADC_Channel_12_Slope);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_13_Slope
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_13_Slope(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_slop+13));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_13_Slope : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_13_Slope(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_13_Slope, 0664, show_ADC_Channel_13_Slope, store_ADC_Channel_13_Slope);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_0_Offset
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_0_Offset(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_offset+0));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_0_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_0_Offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_0_Offset, 0664, show_ADC_Channel_0_Offset, store_ADC_Channel_0_Offset);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_1_Offset
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_1_Offset(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_offset+1));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_1_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_1_Offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_1_Offset, 0664, show_ADC_Channel_1_Offset, store_ADC_Channel_1_Offset);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_2_Offset
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_2_Offset(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_offset+2));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_2_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_2_Offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_2_Offset, 0664, show_ADC_Channel_2_Offset, store_ADC_Channel_2_Offset);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_3_Offset
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_3_Offset(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_offset+3));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_3_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_3_Offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_3_Offset, 0664, show_ADC_Channel_3_Offset, store_ADC_Channel_3_Offset);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_4_Offset
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_4_Offset(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_offset+4));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_4_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_4_Offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_4_Offset, 0664, show_ADC_Channel_4_Offset, store_ADC_Channel_4_Offset);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_5_Offset
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_5_Offset(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_offset+5));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_5_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_5_Offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_5_Offset, 0664, show_ADC_Channel_5_Offset, store_ADC_Channel_5_Offset);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_6_Offset
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_6_Offset(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_offset+6));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_6_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_6_Offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_6_Offset, 0664, show_ADC_Channel_6_Offset, store_ADC_Channel_6_Offset);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_7_Offset
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_7_Offset(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_offset+7));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_7_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_7_Offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_7_Offset, 0664, show_ADC_Channel_7_Offset, store_ADC_Channel_7_Offset);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_8_Offset
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_8_Offset(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_offset+8));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_8_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_8_Offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_8_Offset, 0664, show_ADC_Channel_8_Offset, store_ADC_Channel_8_Offset);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_9_Offset
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_9_Offset(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_offset+9));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_9_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_9_Offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_9_Offset, 0664, show_ADC_Channel_9_Offset, store_ADC_Channel_9_Offset);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_10_Offset
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_10_Offset(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_offset+10));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_10_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_10_Offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_10_Offset, 0664, show_ADC_Channel_10_Offset, store_ADC_Channel_10_Offset);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_11_Offset
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_11_Offset(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_offset+11));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_11_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_11_Offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_11_Offset, 0664, show_ADC_Channel_11_Offset, store_ADC_Channel_11_Offset);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_12_Offset
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_12_Offset(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_offset+12));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_12_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_12_Offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_12_Offset, 0664, show_ADC_Channel_12_Offset, store_ADC_Channel_12_Offset);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_13_Offset
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_13_Offset(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = (*(adc_cali_offset+13));
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_13_Offset : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_13_Offset(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_13_Offset, 0664, show_ADC_Channel_13_Offset, store_ADC_Channel_13_Offset);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : ADC_Channel_Is_Calibration
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_ADC_Channel_Is_Calibration(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=2;
    ret_value = g_ADC_Cali;
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] ADC_Channel_Is_Calibration : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_ADC_Channel_Is_Calibration(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(ADC_Channel_Is_Calibration, 0664, show_ADC_Channel_Is_Calibration, store_ADC_Channel_Is_Calibration);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : Power_On_Voltage
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_Power_On_Voltage(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = Batt_VoltToPercent_Table[0].BattVolt;
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Power_On_Voltage : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_Power_On_Voltage(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(Power_On_Voltage, 0664, show_Power_On_Voltage, store_Power_On_Voltage);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : Power_Off_Voltage
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_Power_Off_Voltage(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = Batt_VoltToPercent_Table[0].BattVolt;
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Power_Off_Voltage : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_Power_Off_Voltage(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(Power_Off_Voltage, 0664, show_Power_Off_Voltage, store_Power_Off_Voltage);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : Charger_TopOff_Value
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_Charger_TopOff_Value(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=1;
    ret_value = Batt_VoltToPercent_Table[10].BattVolt;
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Charger_TopOff_Value : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_Charger_TopOff_Value(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(Charger_TopOff_Value, 0664, show_Charger_TopOff_Value, store_Charger_TopOff_Value);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : FG_Battery_CurrentConsumption
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_FG_Battery_CurrentConsumption(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value=8888;
    ret_value = gFG_current;    
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] FG_Battery_CurrentConsumption : %d/10 mA\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_FG_Battery_CurrentConsumption(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(FG_Battery_CurrentConsumption, 0664, show_FG_Battery_CurrentConsumption, store_FG_Battery_CurrentConsumption);

///////////////////////////////////////////////////////////////////////////////////////////
//// Create File For EM : FG_SW_CoulombCounter
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_FG_SW_CoulombCounter(struct device *dev,struct device_attribute *attr, char *buf)
{
    kal_int32 ret_value=7777;
    ret_value = gFG_columb;
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] FG_SW_CoulombCounter : %d\n", ret_value);
    return sprintf(buf, "%u\n", ret_value);
}
static ssize_t store_FG_SW_CoulombCounter(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[EM] Not Support Write Function\n");    
    return size;
}
static DEVICE_ATTR(FG_SW_CoulombCounter, 0664, show_FG_SW_CoulombCounter, store_FG_SW_CoulombCounter);

///////////////////////////////////////////////////////////////////////////////////////////
//// platform_driver API
///////////////////////////////////////////////////////////////////////////////////////////
#define BAT_MS_TO_NS(x) (x * 1000 * 1000)
static struct hrtimer charger_hv_detect_timer;
static struct task_struct *charger_hv_detect_thread = NULL;
static int charger_hv_detect_flag = 0;
static DECLARE_WAIT_QUEUE_HEAD(charger_hv_detect_waiter);

int charger_hv_detect_sw_thread_handler(void *unused)
{
    ktime_t ktime;

    do
    {
    	ktime = ktime_set(0, BAT_MS_TO_NS(500));

        //xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[charger_hv_detect_sw_thread_handler] \n");

        charger_hv_init();

        wait_event_interruptible(charger_hv_detect_waiter, charger_hv_detect_flag != 0);

        if ((upmu_is_chr_det() == KAL_TRUE))
        {
            check_battery_exist();
        }
    
    	charger_hv_detect_flag = 0;

        if( get_charger_hv_status() == 1)
    	{
            xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[charger_hv_detect_sw_thread_handler] charger hv\n");    
            
            pchr_turn_off_charging_ncp1851();
        }
        else
        {
            //xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[charger_hv_detect_sw_thread_handler] upmu_chr_get_vcdt_hv_det() != 1\n");    
        }

        kick_charger_wdt(); 

        hrtimer_start(&charger_hv_detect_timer, ktime, HRTIMER_MODE_REL);

    } while (!kthread_should_stop());

    return 0;
}

enum hrtimer_restart charger_hv_detect_sw_workaround(struct hrtimer *timer)
{
    charger_hv_detect_flag = 1;
    wake_up_interruptible(&charger_hv_detect_waiter);

    //xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[charger_hv_detect_sw_workaround] \n");

    return HRTIMER_NORESTART;
}

void charger_hv_detect_sw_workaround_init(void)
{
    ktime_t ktime;

    ktime = ktime_set(0, BAT_MS_TO_NS(500));
    hrtimer_init(&charger_hv_detect_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    charger_hv_detect_timer.function = charger_hv_detect_sw_workaround;
    hrtimer_start(&charger_hv_detect_timer, ktime, HRTIMER_MODE_REL);

    charger_hv_detect_thread = kthread_run(charger_hv_detect_sw_thread_handler, 0, "mtk charger_hv_detect_sw_workaround");
    if (IS_ERR(charger_hv_detect_thread))
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[%s]: failed to create charger_hv_detect_sw_workaround thread\n", __FUNCTION__);
    }

    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "charger_hv_detect_sw_workaround_init : done\n" );
}

///////////////////////////////////////////////////////////////////////////////////////////
//// platform_driver API 
///////////////////////////////////////////////////////////////////////////////////////////
static struct hrtimer battery_kthread_timer;
static struct task_struct *battery_kthread_hrtimer_task = NULL;
static int battery_kthread_flag = 0;
static DECLARE_WAIT_QUEUE_HEAD(battery_kthread_waiter);

int battery_kthread_handler(void *unused)
{
    ktime_t ktime;

    do
    {
        ktime = ktime_set(10, 0);	// 10s, 10* 1000 ms
    
//        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[battery_kthread_handler] \n");
        wait_event_interruptible(battery_kthread_waiter, battery_kthread_flag != 0);
    
        battery_kthread_flag = 0;
        bat_thread_wakeup();
        hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);    
        
    } while (!kthread_should_stop());
    
    return 0;
}

enum hrtimer_restart battery_kthread_hrtimer_func(struct hrtimer *timer)
{
    battery_kthread_flag = 1; 
    wake_up_interruptible(&battery_kthread_waiter);

//    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[battery_kthread_hrtimer_func] \n");
    
    return HRTIMER_NORESTART;
}

void battery_kthread_hrtimer_init(void)
{
    ktime_t ktime;

    ktime = ktime_set(10, 0);	// 10s, 10* 1000 ms
    hrtimer_init(&battery_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    battery_kthread_timer.function = battery_kthread_hrtimer_func;    
    hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);

    battery_kthread_hrtimer_task = kthread_run(battery_kthread_handler, NULL, "mtk battery_kthread_handler"); 
    if (IS_ERR(battery_kthread_hrtimer_task))
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[%s]: failed to create battery_kthread_hrtimer_task thread\n", __FUNCTION__);
    }

    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "battery_kthread_hrtimer_init : done\n" );
}

static int mt6320_battery_probe(struct platform_device *dev)    
{
    struct class_device *class_dev = NULL;
    int ret=0;
    int i=0;
    int ret_device_file=0;

    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "******** MT6320 battery driver probe!! ********\n" );

    /* Integrate with NVRAM */
    ret = alloc_chrdev_region(&adc_cali_devno, 0, 1, ADC_CALI_DEVNAME);
    if (ret) 
       xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "Error: Can't Get Major number for adc_cali \n");
    adc_cali_cdev = cdev_alloc();
    adc_cali_cdev->owner = THIS_MODULE;
    adc_cali_cdev->ops = &adc_cali_fops;
    ret = cdev_add(adc_cali_cdev, adc_cali_devno, 1);
    if(ret)
       xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "adc_cali Error: cdev_add\n");
    adc_cali_major = MAJOR(adc_cali_devno);
    adc_cali_class = class_create(THIS_MODULE, ADC_CALI_DEVNAME);
    class_dev = (struct class_device *)device_create(adc_cali_class, 
                                                   NULL, 
                                                   adc_cali_devno, 
                                                   NULL, 
                                                   ADC_CALI_DEVNAME);
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[MT6320 BAT_probe] adc_cali prepare : done !!\n ");

    /* Integrate with Android Battery Service */
    ret = power_supply_register(&(dev->dev), &mt6320_ac_main.psy);
    if (ret)
    {            
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[MT6320 BAT_probe] power_supply_register AC Fail !!\n");                    
        return ret;
    }             
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[MT6320 BAT_probe] power_supply_register AC Success !!\n");

    ret = power_supply_register(&(dev->dev), &mt6320_usb_main.psy);
    if (ret)
    {            
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[MT6320 BAT_probe] power_supply_register USB Fail !!\n");                    
        return ret;
    }             
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[MT6320 BAT_probe] power_supply_register USB Success !!\n");

    ret = power_supply_register(&(dev->dev), &mt6320_battery_main.psy);
    if (ret)
    {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[MT6320 BAT_probe] power_supply_register Battery Fail !!\n");
        return ret;
    }
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[MT6320 BAT_probe] power_supply_register Battery Success !!\n");

//    wake_lock_init(&battery_suspend_lock, WAKE_LOCK_SUSPEND, "battery wakelock");

    /* For EM */
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Charger_Voltage);
    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_0_Slope);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_1_Slope);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_2_Slope);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_3_Slope);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_4_Slope);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_5_Slope);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_6_Slope);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_7_Slope);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_8_Slope);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_9_Slope);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_10_Slope);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_11_Slope);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_12_Slope);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_13_Slope);

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_0_Offset);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_1_Offset);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_2_Offset);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_3_Offset);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_4_Offset);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_5_Offset);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_6_Offset);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_7_Offset);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_8_Offset);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_9_Offset);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_10_Offset);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_11_Offset);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_12_Offset);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_13_Offset);

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_Is_Calibration);

    ret_device_file = device_create_file(&(dev->dev), &dev_attr_Power_On_Voltage);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_Power_Off_Voltage);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_Charger_TopOff_Value);
    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Battery_CurrentConsumption);
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_SW_CoulombCounter);

    /* Initialization BMT Struct */
    for (i=0; i<BATTERY_AVERAGE_SIZE; i++) {
        batteryCurrentBuffer[i] = 0;
        batteryVoltageBuffer[i] = 0; 
        batterySOCBuffer[i] = 0;
        batteryTempBuffer[i] = 0;
    }
    batteryVoltageSum = 0;
    batteryCurrentSum = 0;
    batterySOCSum = 0;
    batteryTempSum = 0;

    BMT_status.bat_exist = 1;       /* phone must have battery */
    BMT_status.charger_exist = 0;     /* for default, no charger */
    BMT_status.bat_vol = 0;
    BMT_status.ICharging = 0;
    BMT_status.temperature = 0;
    BMT_status.charger_vol = 0;
    BMT_status.total_charging_time = 0;
    BMT_status.PRE_charging_time = 0;
    BMT_status.CC_charging_time = 0;
    BMT_status.TOPOFF_charging_time = 0;
    BMT_status.POSTFULL_charging_time = 0;

    BMT_status.bat_charging_state = CHR_PRE;

    //baton initial setting
    //ret=pmic_config_interface(CHR_CON7, 0x01, PMIC_BATON_TDET_EN_MASK, PMIC_BATON_TDET_EN_SHIFT); //BATON_TDET_EN=1
    //ret=pmic_config_interface(AUXADC_CON0, 0x01, PMIC_RG_BUF_PWD_B_MASK, PMIC_RG_BUF_PWD_B_SHIFT); //RG_BUF_PWD_B=1
    //xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[mt6320_battery_probe] BATON_TDET_EN=1 & RG_BUF_PWD_B=1\n");

    //battery kernel thread for 10s check and charger in/out event
    /* Replace GPT timer by hrtime */
    battery_kthread_hrtimer_init();     
    kthread_run(bat_thread_kthread, NULL, "bat_thread_kthread"); 
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "[mt6320_battery_probe] bat_thread_kthread Done\n");    
    
    charger_hv_detect_sw_workaround_init();

    /*LOG System Set*/
    init_proc_log();

    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "DCT-PMIC-ADC:AUXADC_BATTERY_VOLTAGE_CHANNEL=%d\r\n",AUXADC_BATTERY_VOLTAGE_CHANNEL);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "DCT-PMIC-ADC:AUXADC_REF_CURRENT_CHANNEL=%d\r\n",AUXADC_REF_CURRENT_CHANNEL);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "DCT-PMIC-ADC:AUXADC_CHARGER_VOLTAGE_CHANNEL=%d\r\n",AUXADC_CHARGER_VOLTAGE_CHANNEL);
    xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "DCT-PMIC-ADC:AUXADC_TEMPERATURE_CHANNEL=%d\r\n",AUXADC_TEMPERATURE_CHANNEL);

    g_bat_init_flag=1;
	
    return 0;
}
#endif

static int mt6320_battery_remove(struct platform_device *dev)    
{
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "******** MT6320 battery driver remove!! ********\n" );

    return 0;
}

static void mt6320_battery_shutdown(struct platform_device *dev)    
{
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "******** MT6320 battery driver shutdown!! ********\n" );

}

static int mt6320_battery_suspend(struct platform_device *dev, pm_message_t state)    
{
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "******** MT6320 battery driver suspend!! ********\n" );

    return 0;
}

int force_get_tbat(void)
{
#if defined(CONFIG_POWER_EXT)
    return 19;
#else
    int bat_temperature_volt=0;
    int bat_temperature_val=0;
    int fg_r_value=0;
    kal_int32 fg_current_temp=0;
    kal_bool fg_current_state=KAL_FALSE;
    int bat_temperature_volt_temp=0;
    
    /* Get V_BAT_Temperature */
    bat_temperature_volt = get_tbat_volt(2); 
    if(bat_temperature_volt != 0)
    {   
        if( gForceADCsolution == 1 )
        {
            /*Use no gas gauge*/
        }
        else
        {
            fg_r_value = get_r_fg_value();
            fg_current_temp = fgauge_read_current();
            fg_current_temp = fg_current_temp/10;
            fg_current_state = get_gFG_Is_Charging();
            if(fg_current_state==KAL_TRUE)
            {
                bat_temperature_volt_temp = bat_temperature_volt;
                bat_temperature_volt = bat_temperature_volt - ((fg_current_temp*fg_r_value)/1000);
            }
            else
            {
                bat_temperature_volt_temp = bat_temperature_volt;
                bat_temperature_volt = bat_temperature_volt + ((fg_current_temp*fg_r_value)/1000);
            }
        }
        
        bat_temperature_val = BattVoltToTemp(bat_temperature_volt);        
    }
    
    printk(KERN_CRIT "[tbat] %d,%d,%d,%d,%d,%d\n", 
        bat_temperature_volt_temp, bat_temperature_volt, fg_current_state, fg_current_temp, fg_r_value, bat_temperature_val);
    
    return bat_temperature_val;    
#endif    
}
EXPORT_SYMBOL(force_get_tbat);

static int mt6320_battery_resume(struct platform_device *dev)
{
    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "******** MT6320 battery driver resume!! ********\n" );

	//g_battery_flag_resume=1;

    return 0;
}

struct platform_device MT6320_battery_device = {
    .name   = "mt6320-battery",
    .id	    = -1,
};

static struct platform_driver mt6320_battery_driver = {
    .probe         = mt6320_battery_probe,
    .remove        = mt6320_battery_remove,
    .shutdown      = mt6320_battery_shutdown,
    //#ifdef CONFIG_PM
    .suspend       = mt6320_battery_suspend,
    .resume        = mt6320_battery_resume,
    //#endif
    .driver     = {
        .name = "mt6320-battery",
    },
};

///////////////////////////////////////////////////////////////////////////////////////////
//// Battery Notify API
///////////////////////////////////////////////////////////////////////////////////////////
static ssize_t show_BatteryNotify(struct device *dev,struct device_attribute *attr, char *buf)
{
	if (Enable_BATDRV_LOG == 1) {
	xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[Battery] show_BatteryNotify : %x\n", g_BatteryNotifyCode);
	}
	return sprintf(buf, "%u\n", g_BatteryNotifyCode);
}
static ssize_t store_BatteryNotify(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	char *pvalue = NULL;
	unsigned int reg_BatteryNotifyCode = 0;
	xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[Battery] store_BatteryNotify\n");
	if(buf != NULL && size != 0)
	{
		xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[Battery] buf is %s and size is %d \n",buf,size);
		reg_BatteryNotifyCode = simple_strtoul(buf,&pvalue,16);
		g_BatteryNotifyCode = reg_BatteryNotifyCode;
		xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[Battery] store code : %x \n",g_BatteryNotifyCode);
	}
	return size;
}
static DEVICE_ATTR(BatteryNotify, 0664, show_BatteryNotify, store_BatteryNotify);

static ssize_t show_BN_TestMode(struct device *dev,struct device_attribute *attr, char *buf)
{
	xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[Battery] show_BN_TestMode : %x\n", g_BN_TestMode);
	return sprintf(buf, "%u\n", g_BN_TestMode);
}
static ssize_t store_BN_TestMode(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	char *pvalue = NULL;
	unsigned int reg_BN_TestMode = 0;
	xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[Battery] store_BN_TestMode\n");
	if(buf != NULL && size != 0)
	{
		xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[Battery] buf is %s and size is %d \n",buf,size);
		reg_BN_TestMode = simple_strtoul(buf,&pvalue,16);
		g_BN_TestMode = reg_BN_TestMode;
		xlog_printk(ANDROID_LOG_DEBUG, "Power/Battery", "[Battery] store g_BN_TestMode : %x \n",g_BN_TestMode);
	}
	return size;
}
static DEVICE_ATTR(BN_TestMode, 0664, show_BN_TestMode, store_BN_TestMode);

///////////////////////////////////////////////////////////////////////////////////////////
//// platform_driver API
///////////////////////////////////////////////////////////////////////////////////////////
static int battery_cmd_read(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    char *p = buf;

    p += sprintf(p, "g_battery_thermal_throttling_flag=%d,\nbattery_cmd_thermal_test_mode=%d,\nbattery_cmd_thermal_test_mode_value=%d\n",
		g_battery_thermal_throttling_flag, battery_cmd_thermal_test_mode, battery_cmd_thermal_test_mode_value);

    *start = buf + off;

    len = p - buf;
    if (len > off)
        len -= off;
    else
        len = 0;

    return len < count ? len  : count;
}

static ssize_t battery_cmd_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    int len = 0, bat_tt_enable=0, bat_thr_test_mode=0, bat_thr_test_value=0;
    char desc[32];

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len))
    {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%d %d %d", &bat_tt_enable, &bat_thr_test_mode, &bat_thr_test_value) == 3)
    {
        g_battery_thermal_throttling_flag = bat_tt_enable;
        battery_cmd_thermal_test_mode = bat_thr_test_mode;
		battery_cmd_thermal_test_mode_value = bat_thr_test_value;

        xlog_printk(ANDROID_LOG_DEBUG, "Power/Thermal", "bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
			g_battery_thermal_throttling_flag, battery_cmd_thermal_test_mode, battery_cmd_thermal_test_mode_value);

        return count;
    }
    else
    {
        xlog_printk(ANDROID_LOG_DEBUG, "Power/Thermal", "  bad argument, echo [bat_tt_enable] [bat_thr_test_mode] [bat_thr_test_value] > battery_cmd\n");
    }

    return -EINVAL;
}

static int mt_batteryNotify_probe(struct platform_device *dev)
{
	int ret_device_file = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *battery_dir = NULL;

    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "******** mt_batteryNotify_probe!! ********\n" );

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BatteryNotify);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BN_TestMode);

    battery_dir = proc_mkdir("mtk_battery_cmd", NULL);
    if (!battery_dir)
    {
        pr_err("[%s]: mkdir /proc/mtk_battery_cmd failed\n", __FUNCTION__);
    }
    else
    {
        entry = create_proc_entry("battery_cmd", S_IRUGO | S_IWUSR, battery_dir);
        if (entry)
        {
            entry->read_proc = battery_cmd_read;
            entry->write_proc = battery_cmd_write;
        }
    }

	xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "******** mtk_battery_cmd!! ********\n" );

    return 0;
}

struct platform_device MT_batteryNotify_device = {
    .name   = "mt-battery",
    .id	    = -1,
};

static struct platform_driver mt_batteryNotify_driver = {
    .probe		= mt_batteryNotify_probe,
    .driver     = {
        .name = "mt-battery",
    },
};

static int __init mt6320_battery_init(void)
{
    int ret;

    ret = platform_device_register(&MT6320_battery_device);
    if (ret) {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "****[mt6320_battery_driver] Unable to device register(%d)\n", ret);
	return ret;
    }
    
    ret = platform_driver_register(&mt6320_battery_driver);
    if (ret) {
        xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "****[mt6320_battery_driver] Unable to register driver (%d)\n", ret);
        return ret;
    }

	// battery notofy UI
	ret = platform_device_register(&MT_batteryNotify_device);
    if (ret) {
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "****[mt_batteryNotify] Unable to device register(%d)\n", ret);
		return ret;
    }
    ret = platform_driver_register(&mt_batteryNotify_driver);
    if (ret) {
		xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "****[mt_batteryNotify] Unable to register driver (%d)\n", ret);
		return ret;
    }

    xlog_printk(ANDROID_LOG_INFO, "Power/Battery", "****[mt6320_battery_driver] Initialization : DONE !!\n");

    return 0;
}

static void __exit mt6320_battery_exit (void)
{
}

module_init(mt6320_battery_init);
module_exit(mt6320_battery_exit);

MODULE_AUTHOR("YT Lee");
MODULE_DESCRIPTION("MT6320 Battery Device Driver NCP1851");
MODULE_LICENSE("GPL");

