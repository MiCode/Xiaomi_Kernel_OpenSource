#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <asm/uaccess.h>
#include "mach/mtk_thermal_monitor.h"
#include "mach/mt_typedefs.h"
#include "mach/mt_thermal.h"

extern struct proc_dir_entry * mtk_thermal_get_proc_drv_therm_dir_entry(void);

static unsigned int interval = 0; /* seconds, 0 : no auto polling */
static int trip_temp[10] = {120000,110000,100000,90000,80000,70000,65000,60000,55000,50000};
static struct thermal_zone_device *thz_dev;
static int mtkts_btsmdpa_debug_log = 0;
static int kernelmode = 0;
static int g_THERMAL_TRIP[10] = {0,0,0,0,0,0,0,0,0,0};
static int num_trip=0;
static char g_bind0[20]={0};
static char g_bind1[20]={0};
static char g_bind2[20]={0};
static char g_bind3[20]={0};
static char g_bind4[20]={0};
static char g_bind5[20]={0};
static char g_bind6[20]={0};
static char g_bind7[20]={0};
static char g_bind8[20]={0};
static char g_bind9[20]={0};

/**
 * If curr_temp >= polling_trip_temp1, use interval
 * else if cur_temp >= polling_trip_temp2 && curr_temp < polling_trip_temp1, use interval*polling_factor1
 * else, use interval*polling_factor2
 */
static int polling_trip_temp1 = 40000;
static int polling_trip_temp2 = 20000;
static int polling_factor1 = 5000;
static int polling_factor2 = 10000;


#define MTKTS_BTSMDPA_TEMP_CRIT 60000 /* 60.000 degree Celsius */


// 1: turn on arbitration reasonable temo; 0: turn off
#define AUTO_ARBITRATION_REASONABLE_TEMP (0)


#if AUTO_ARBITRATION_REASONABLE_TEMP
#define XTAL_BTSMDPA_TEMP_DIFF 10000  //10 degree

extern int mtktsxtal_get_xtal_temp(void);
#endif


#define mtkts_btsmdpa_dprintk(fmt, args...)   \
do {                                    \
	if (mtkts_btsmdpa_debug_log) {                \
		xlog_printk(ANDROID_LOG_INFO, "Power/BTSMDPA_Thermal", fmt, ##args); \
	}                                   \
} while(0)


//#define INPUT_PARAM_FROM_USER_AP

/*
 * kernel fopen/fclose
 */
/*
static mm_segment_t oldfs;

static void my_close(int fd)
{
	set_fs(oldfs);
	sys_close(fd);
}

static int my_open(char *fname, int flag)
{
	oldfs = get_fs();
    set_fs(KERNEL_DS);
    return sys_open(fname, flag, 0);
}
*/
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
extern int IMM_IsAdcInitReady(void);
typedef struct{
    INT32 BTSMDPA_Temp;
    INT32 TemperatureR;
}BTSMDPA_TEMPERATURE;


#define AUX_IN12_NTC (12)
/*
AuxADC channel ID	Description
Channel 0	External use (AUXADC0)
Channel 1	External use (AUXADC1)
Channel 2	Internal use
Channel 3	Internal use
Channel 4	Internal use
Channel 5	Internal use
Channel 6	Internal use
Channel 7	Internal use
Channel 8	Internal use
Channel 9	Internal use
Channel 10	Internal use ( T-Sensors Usage )
Channel 11	Internal use ( T-Sensors Usage )
Channel 12	External use (AUX_IN 2)

*/

#if 0
static int g_RAP_pull_up_R = 390000;//390K,pull up resister
static int g_TAP_over_critical_low =4251000 ;//base on 100K NTC temp default value -40 deg
static int g_RAP_pull_up_voltage = 1800;//1.8V ,pull up voltage
static int g_RAP_ntc_table = 6;  //default is //NTCG104EF104F(100K)
static int g_RAP_ADC_channel = AUX_IN1_NTC;  //default is 0
#else
static int g_RAP_pull_up_R = 39000;//39K,pull up resister
static int g_TAP_over_critical_low = 188500;//base on 10K NTC temp default value -40 deg
static int g_RAP_pull_up_voltage = 1800;//1.8V ,pull up voltage
static int g_RAP_ntc_table = 4;  //default is AP_NTC_10
static int g_RAP_ADC_channel = AUX_IN12_NTC;  //default is 0
#endif

static int g_btsmdpa_TemperatureR = 0;
//BTSMDPA_TEMPERATURE BTSMDPA_Temperature_Table[] = {0};

static BTSMDPA_TEMPERATURE BTSMDPA_Temperature_Table[] = {
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
   	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0}
};


//AP_NTC_BL197
BTSMDPA_TEMPERATURE BTSMDPA_Temperature_Table1[] = {
    {-40,74354},//FIX_ME
    {-35,74354},//FIX_ME
    {-30,74354},//FIX_ME
    {-25,74354},//FIX_ME
	{-20,74354},
	{-15,57626},
	{-10,45068},
	{ -5,35548},
	{  0,28267},
	{  5,22650},
	{ 10,18280},
	{ 15,14855},
	{ 20,12151},
	{ 25,10000},//10K
	{ 30,8279},
	{ 35,6892},
	{ 40,5768},
	{ 45,4852},
	{ 50,4101},
	{ 55,3483},
	{ 60,2970},//FIX_ME
	{ 60,2970},//FIX_ME
	{ 60,2970},//FIX_ME
	{ 60,2970},//FIX_ME
	{ 60,2970},//FIX_ME
	{ 60,2970},//FIX_ME
	{ 60,2970},//FIX_ME
	{ 60,2970},//FIX_ME
	{ 60,2970},//FIX_ME
	{ 60,2970},//FIX_ME
	{ 60,2970},//FIX_ME
	{ 60,2970},//FIX_ME
	{ 60,2970},//FIX_ME
	{ 60,2970} //FIX_ME
};

//AP_NTC_TSM_1
BTSMDPA_TEMPERATURE BTSMDPA_Temperature_Table2[] = {
    {-40,70603},//FIX_ME
    {-35,70603},//FIX_ME
    {-30,70603},//FIX_ME
    {-25,70603},//FIX_ME
	{-20,70603},
	{-15,55183},
	{-10,43499},
	{ -5,34569},
	{  0,27680},
	{  5,22316},
	{ 10,18104},
	{ 15,14773},
	{ 20,12122},
	{ 25,10000},//10K
	{ 30,8294},
	{ 35,6915},
	{ 40,5795},
	{ 45,4882},
	{ 50,4133},
	{ 55,3516},
	{ 60,3004},//FIX_ME
	{ 60,3004},//FIX_ME
	{ 60,3004},//FIX_ME
	{ 60,3004},//FIX_ME
	{ 60,3004},//FIX_ME
	{ 60,3004},//FIX_ME
	{ 60,3004},//FIX_ME
	{ 60,3004},//FIX_ME
	{ 60,3004},//FIX_ME
	{ 60,3004},//FIX_ME
	{ 60,3004},//FIX_ME
	{ 60,3004},//FIX_ME
	{ 60,3004},//FIX_ME
	{ 60,3004} //FIX_ME
};

//AP_NTC_10_SEN_1
BTSMDPA_TEMPERATURE BTSMDPA_Temperature_Table3[] = {
     {-40,74354},//FIX_ME
     {-35,74354},//FIX_ME
     {-30,74354},//FIX_ME
	 {-25,74354},//FIX_ME
	 {-20,74354},
	 {-15,57626},
	 {-10,45068},
	 { -5,35548},
	 {  0,28267},
	 {  5,22650},
	 { 10,18280},
	 { 15,14855},
	 { 20,12151},
	 { 25,10000},//10K
	 { 30,8279},
	 { 35,6892},
	 { 40,5768},
	 { 45,4852},
	 { 50,4101},
	 { 55,3483},
	 { 60,2970},
	 { 60,2970},//FIX_ME
	 { 60,2970},//FIX_ME
	 { 60,2970},//FIX_ME
	 { 60,2970},//FIX_ME
	 { 60,2970},//FIX_ME
	 { 60,2970},//FIX_ME
	 { 60,2970},//FIX_ME
	 { 60,2970},//FIX_ME
	 { 60,2970},//FIX_ME
	 { 60,2970},//FIX_ME
	 { 60,2970},//FIX_ME
	 { 60,2970},//FIX_ME
	 { 60,2970} //FIX_ME
};
#if 0
//AP_NTC_10
BTSMDPA_TEMPERATURE BTSMDPA_Temperature_Table4[] = {
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
#else
//AP_NTC_10(TSM0A103F34D1RZ)
BTSMDPA_TEMPERATURE BTSMDPA_Temperature_Table4[] = {
    {-40,188500},
    {-35,144290},
    {-30,111330},
	{-25,86560},
    {-20,67790},
    {-15,53460},
    {-10,42450},
    { -5,33930},
    {  0,27280},
    {  5,22070},
    { 10,17960},
    { 15,14700},
    { 20,12090},
    { 25,10000},//10K
    { 30,8310},
    { 35,6940},
    { 40,5830},
    { 45,4910},
    { 50,4160},
    { 55,3540},
    { 60,3020},
    { 65,2590},
    { 70,2230},
    { 75,1920},
    { 80,1670},
    { 85,1450},
    { 90,1270},
    { 95,1110},
    { 100,975},
    { 105,860},
    { 110,760},
    { 115,674},
    { 120,599},
    { 125,534}
};
#endif

//AP_NTC_47
BTSMDPA_TEMPERATURE BTSMDPA_Temperature_Table5[] = {
    {-40,483954},//FIX_ME
    {-35,483954},//FIX_ME
    {-30,483954},//FIX_ME
	{-25,483954},//FIX_ME
    {-20,483954},
    {-15,360850},
    {-10,271697},
    { -5,206463},
    {  0,158214},
    {  5,122259},
    { 10,95227},
    { 15,74730},
    { 20,59065},
    { 25,47000},//47K
    { 30,37643},
    { 35,30334},
    { 40,24591},
    { 45,20048},
    { 50,16433},
    { 55,13539},
    { 60,11210},
    { 60,11210},//FIX_ME
    { 60,11210},//FIX_ME
    { 60,11210},//FIX_ME
    { 60,11210},//FIX_ME
    { 60,11210},//FIX_ME
    { 60,11210},//FIX_ME
    { 60,11210},//FIX_ME
    { 60,11210},//FIX_ME
    { 60,11210},//FIX_ME
    { 60,11210},//FIX_ME
    { 60,11210},//FIX_ME
    { 60,11210},//FIX_ME
    { 60,11210} //FIX_ME
};


//NTCG104EF104F(100K)
BTSMDPA_TEMPERATURE BTSMDPA_Temperature_Table6[] = {
    {-40,4251000},
    {-35,3005000},
    {-30,2149000},
    {-25,1554000},
    {-20,1135000},
    {-15,837800},
    {-10,624100},
    { -5,469100},
    {  0,355600},
    {  5,271800},
    { 10,209400},
    { 15,162500},
    { 20,127000},
    { 25,100000},//100K
    { 30,79230},
    { 35,63180},
    { 40,50680},
    { 45,40900},
    { 50,33190},
    { 55,27090},
    { 60,22220},
    { 65,18320},
    { 70,15180},
    { 75,12640},
    { 80,10580},
    { 85, 8887},
    { 90, 7500},
    { 95, 6357},
    { 100,5410},
    { 105,4623},
    { 110,3965},
    { 115,3415},
    { 120,2951},
    { 125,2560}
};



/* convert register to temperature  */
static INT16 mtkts_btsmdpa_thermistor_conver_temp(INT32 Res)
{
    int i=0;
    int asize=0;
    INT32 RES1=0,RES2=0;
    INT32 TAP_Value=-200,TMP1=0,TMP2=0;

	asize=(sizeof(BTSMDPA_Temperature_Table)/sizeof(BTSMDPA_TEMPERATURE));
	//xlog_printk(ANDROID_LOG_INFO, "Power/BTSMDPA_Thermal", "mtkts_btsmdpa_thermistor_conver_temp() : asize = %d, Res = %d\n",asize,Res);
    if(Res>=BTSMDPA_Temperature_Table[0].TemperatureR)
    {
        TAP_Value = -40;//min
    }
    else if(Res<=BTSMDPA_Temperature_Table[asize-1].TemperatureR)
    {
        TAP_Value = 125;//max
    }
    else
    {
        RES1=BTSMDPA_Temperature_Table[0].TemperatureR;
        TMP1=BTSMDPA_Temperature_Table[0].BTSMDPA_Temp;
		//xlog_printk(ANDROID_LOG_INFO, "Power/BTSMDPA_Thermal", "%d : RES1 = %d,TMP1 = %d\n",__LINE__,RES1,TMP1);

        for(i=0;i < asize;i++)
        {
            if(Res>=BTSMDPA_Temperature_Table[i].TemperatureR)
            {
                RES2=BTSMDPA_Temperature_Table[i].TemperatureR;
                TMP2=BTSMDPA_Temperature_Table[i].BTSMDPA_Temp;
                //xlog_printk(ANDROID_LOG_INFO, "Power/BTSMDPA_Thermal", "%d :i=%d, RES2 = %d,TMP2 = %d\n",__LINE__,i,RES2,TMP2);
                break;
            }
            else
            {
                RES1=BTSMDPA_Temperature_Table[i].TemperatureR;
                TMP1=BTSMDPA_Temperature_Table[i].BTSMDPA_Temp;
                //xlog_printk(ANDROID_LOG_INFO, "Power/BTSMDPA_Thermal", "%d :i=%d, RES1 = %d,TMP1 = %d\n",__LINE__,i,RES1,TMP1);
            }
        }

        TAP_Value = (((Res-RES2)*TMP1)+((RES1-Res)*TMP2))/(RES1-RES2);
    }

    #if 0
    xlog_printk(ANDROID_LOG_INFO, "Power/BTSMDPA_Thermal", "mtkts_btsmdpa_thermistor_conver_temp() : TAP_Value = %d\n",TAP_Value);
    xlog_printk(ANDROID_LOG_INFO, "Power/BTSMDPA_Thermal", "mtkts_btsmdpa_thermistor_conver_temp() : Res = %d\n",Res);
    xlog_printk(ANDROID_LOG_INFO, "Power/BTSMDPA_Thermal", "mtkts_btsmdpa_thermistor_conver_temp() : RES1 = %d\n",RES1);
    xlog_printk(ANDROID_LOG_INFO, "Power/BTSMDPA_Thermal", "mtkts_btsmdpa_thermistor_conver_temp() : RES2 = %d\n",RES2);
    xlog_printk(ANDROID_LOG_INFO, "Power/BTSMDPA_Thermal", "mtkts_btsmdpa_thermistor_conver_temp() : TMP1 = %d\n",TMP1);
    xlog_printk(ANDROID_LOG_INFO, "Power/BTSMDPA_Thermal", "mtkts_btsmdpa_thermistor_conver_temp() : TMP2 = %d\n",TMP2);
    #endif

    return TAP_Value;
}

/* convert ADC_AP_temp_volt to register */
/*Volt to Temp formula same with 6589*/
static INT16 mtk_ts_btsmdpa_volt_to_temp(UINT32 dwVolt)
{
    INT32 TRes;
    INT32 dwVCriAP = 0;
    INT32 BTSMDPA_TMP = -100;

    //SW workaround-----------------------------------------------------
    //dwVCriAP = (TAP_OVER_CRITICAL_LOW * 1800) / (TAP_OVER_CRITICAL_LOW + 39000);
    //dwVCriAP = (TAP_OVER_CRITICAL_LOW * RAP_PULL_UP_VOLT) / (TAP_OVER_CRITICAL_LOW + RAP_PULL_UP_R);
    dwVCriAP = (g_TAP_over_critical_low * g_RAP_pull_up_voltage) / (g_TAP_over_critical_low + g_RAP_pull_up_R);

    if(dwVolt > dwVCriAP)
    {
        TRes = g_TAP_over_critical_low;
    }
    else
    {
        //TRes = (39000*dwVolt) / (1800-dwVolt);
       // TRes = (RAP_PULL_UP_R*dwVolt) / (RAP_PULL_UP_VOLT-dwVolt);
        TRes = (g_RAP_pull_up_R*dwVolt) / (g_RAP_pull_up_voltage-dwVolt);
    }
    //------------------------------------------------------------------

    g_btsmdpa_TemperatureR = TRes;

    /* convert register to temperature */
    BTSMDPA_TMP = mtkts_btsmdpa_thermistor_conver_temp(TRes);

    return BTSMDPA_TMP;
}

static int get_hw_btsmdpa_temp(void)
{

	int ret = 0, data[4], i, ret_value = 0, ret_temp = 0, output;
	int times=1, Channel=g_RAP_ADC_channel;

	if( IMM_IsAdcInitReady() == 0 )
	{
        printk("[thermal_auxadc_get_data]: AUXADC is not ready\n");
		return 0;
	}

	i = times;
	while (i--)
	{
		ret_value = IMM_GetOneChannelValue(Channel, data, &ret_temp);
		ret += ret_temp;
		mtkts_btsmdpa_dprintk("[thermal_auxadc_get_data(AUX_IN12_NTC)]: ret_temp=%d\n",ret_temp);
        mtkts_btsmdpa_dprintk("[thermal_auxadc_get_data(AUX_IN12_NTC)]: ret_temp=%d\n",ret_temp);
	}


	//ret = ret*1500/4096	;
	ret = ret*1800/4096;//82's ADC power
	mtkts_btsmdpa_dprintk("APtery output mV = %d\n",ret);
	output = mtk_ts_btsmdpa_volt_to_temp(ret);
	mtkts_btsmdpa_dprintk("BTSMDPA output temperature = %d\n",output);
	return output;
}

static DEFINE_MUTEX(BTSMDPA_lock);
int ts_btsmdpa_at_boot_time=0;
int mtkts_btsmdpa_get_hw_temp(void)
{
	int t_ret=0;
//	static int AP[60]={0};
//	int i=0;

	mutex_lock(&BTSMDPA_lock);

    //get HW AP temp (TSAP)
    //cat /sys/class/power_supply/AP/AP_temp
	t_ret = get_hw_btsmdpa_temp();
	t_ret = t_ret * 1000;

	mutex_unlock(&BTSMDPA_lock);

    //if (t_ret > 60000) // abnormal high temp
    //    printk("[Power/BTSMDPA_Thermal] T_btsmdpa=%d\n", t_ret);

	mtkts_btsmdpa_dprintk("T_btsmdpa %d\n", t_ret);
	
	return t_ret;
}

static int mtkts_btsmdpa_get_temp(struct thermal_zone_device *thermal,
			                 unsigned long *t)
{
	*t = mtkts_btsmdpa_get_hw_temp();

	if ((int) *t > 52000)
	    xlog_printk(ANDROID_LOG_INFO, "Power/BTSMDPA_Thermal", "T=%d\n", (int) *t);

    if ((int) *t >= polling_trip_temp1)
        thermal->polling_delay = interval*1000;
    else if ((int) *t < polling_trip_temp2)
        thermal->polling_delay = interval * polling_factor2;
    else
        thermal->polling_delay = interval * polling_factor1;

	return 0;
}

static int mtkts_btsmdpa_bind(struct thermal_zone_device *thermal,
			struct thermal_cooling_device *cdev)
{
	int table_val=0;

	if(!strcmp(cdev->type, g_bind0))
	{
		table_val = 0;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind1))
	{
		table_val = 1;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind2))
	{
		table_val = 2;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind3))
	{
		table_val = 3;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind4))
	{
		table_val = 4;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind5))
	{
		table_val = 5;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind6))
	{
		table_val = 6;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind7))
	{
		table_val = 7;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind8))
	{
		table_val = 8;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind9))
	{
		table_val = 9;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_bind] %s\n", cdev->type);
	}
	else
	{
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_bind] error binding cooling dev\n");
		return -EINVAL;
	} else {
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_bind] binding OK, %d\n", table_val);
	}

	return 0;
}

static int mtkts_btsmdpa_unbind(struct thermal_zone_device *thermal,
			  struct thermal_cooling_device *cdev)
{
    int table_val=0;

	if(!strcmp(cdev->type, g_bind0))
	{
		table_val = 0;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind1))
	{
		table_val = 1;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind2))
	{
		table_val = 2;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind3))
	{
		table_val = 3;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind4))
	{
		table_val = 4;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind5))
	{
		table_val = 5;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind6))
	{
		table_val = 6;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind7))
	{
		table_val = 7;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind8))
	{
		table_val = 8;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind9))
	{
		table_val = 9;
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_unbind] %s\n", cdev->type);
	}
	else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_unbind] error unbinding cooling dev\n");
		return -EINVAL;
	} else {
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_unbind] unbinding OK\n");
	}

	return 0;
}

static int mtkts_btsmdpa_get_mode(struct thermal_zone_device *thermal,
			    enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED
			     : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtkts_btsmdpa_set_mode(struct thermal_zone_device *thermal,
			    enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtkts_btsmdpa_get_trip_type(struct thermal_zone_device *thermal, int trip,
				 enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtkts_btsmdpa_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				 unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtkts_btsmdpa_get_crit_temp(struct thermal_zone_device *thermal,
				 unsigned long *temperature)
{
	*temperature = MTKTS_BTSMDPA_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtkts_btsmdpa_dev_ops = {
	.bind = mtkts_btsmdpa_bind,
	.unbind = mtkts_btsmdpa_unbind,
	.get_temp = mtkts_btsmdpa_get_temp,
	.get_mode = mtkts_btsmdpa_get_mode,
	.set_mode = mtkts_btsmdpa_set_mode,
	.get_trip_type = mtkts_btsmdpa_get_trip_type,
	.get_trip_temp = mtkts_btsmdpa_get_trip_temp,
	.get_crit_temp = mtkts_btsmdpa_get_crit_temp,
};



static int mtkts_btsmdpa_read(struct seq_file *m, void *v)
{

	seq_printf(m, "[mtkts_btsmdpa_read] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d,\n\
				trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,\n\
				g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,\n\
				g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n\
				cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\n\
				cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s,time_ms=%d\n",
				trip_temp[0],trip_temp[1],trip_temp[2],trip_temp[3],trip_temp[4],
				trip_temp[5],trip_temp[6],trip_temp[7],trip_temp[8],trip_temp[9],
				g_THERMAL_TRIP[0],g_THERMAL_TRIP[1],g_THERMAL_TRIP[2],g_THERMAL_TRIP[3],g_THERMAL_TRIP[4],
				g_THERMAL_TRIP[5],g_THERMAL_TRIP[6],g_THERMAL_TRIP[7],g_THERMAL_TRIP[8],g_THERMAL_TRIP[9],
				g_bind0,g_bind1,g_bind2,g_bind3,g_bind4,g_bind5,g_bind6,g_bind7,g_bind8,g_bind9,
				interval*1000);




	return 0;
}

int mtkts_btsmdpa_register_thermal(void);
void mtkts_btsmdpa_unregister_thermal(void);

static ssize_t mtkts_btsmdpa_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
#if AUTO_ARBITRATION_REASONABLE_TEMP
	int Ap_temp=0,XTAL_temp=0,CPU_Tj=0;
    int AP_XTAL_diff=0;
#endif
	int len=0,time_msec=0;
	int trip[10]={0};
	int t_type[10]={0};
	int i;
	char bind0[20],bind1[20],bind2[20],bind3[20],bind4[20];
	char bind5[20],bind6[20],bind7[20],bind8[20],bind9[20];
	char desc[512];



	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
	{
		return 0;
	}
	desc[len] = '\0';

	if (sscanf(desc, "%d %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d",
				&num_trip, &trip[0],&t_type[0],bind0, &trip[1],&t_type[1],bind1,
				&trip[2],&t_type[2],bind2, &trip[3],&t_type[3],bind3,
				&trip[4],&t_type[4],bind4, &trip[5],&t_type[5],bind5,
				&trip[6],&t_type[6],bind6, &trip[7],&t_type[7],bind7,
				&trip[8],&t_type[8],bind8, &trip[9],&t_type[9],bind9,
				&time_msec) == 32)
	{
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_write] mtkts_btsmdpa_unregister_thermal\n");
		mtkts_btsmdpa_unregister_thermal();

		for(i=0; i<num_trip; i++)
			g_THERMAL_TRIP[i] = t_type[i];

		g_bind0[0]=g_bind1[0]=g_bind2[0]=g_bind3[0]=g_bind4[0]=g_bind5[0]=g_bind6[0]=g_bind7[0]=g_bind8[0]=g_bind9[0]='\0';

		for(i=0; i<20; i++)
		{
			g_bind0[i]=bind0[i];
			g_bind1[i]=bind1[i];
			g_bind2[i]=bind2[i];
			g_bind3[i]=bind3[i];
			g_bind4[i]=bind4[i];
			g_bind5[i]=bind5[i];
			g_bind6[i]=bind6[i];
			g_bind7[i]=bind7[i];
			g_bind8[i]=bind8[i];
			g_bind9[i]=bind9[i];
		}

		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_write] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,\
					g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
				g_THERMAL_TRIP[0],g_THERMAL_TRIP[1],g_THERMAL_TRIP[2],g_THERMAL_TRIP[3],g_THERMAL_TRIP[4],
				g_THERMAL_TRIP[5],g_THERMAL_TRIP[6],g_THERMAL_TRIP[7],g_THERMAL_TRIP[8],g_THERMAL_TRIP[9]);
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_write] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\
					cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
				g_bind0,g_bind1,g_bind2,g_bind3,g_bind4,g_bind5,g_bind6,g_bind7,g_bind8,g_bind9);

		for(i=0; i<num_trip; i++)
		{
			trip_temp[i]=trip[i];
		}

		interval=time_msec / 1000;

		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_write] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d,\
				trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,time_ms=%d\n",
				trip_temp[0],trip_temp[1],trip_temp[2],trip_temp[3],trip_temp[4],
				trip_temp[5],trip_temp[6],trip_temp[7],trip_temp[8],trip_temp[9],interval*1000);

		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_write] mtkts_btsmdpa_register_thermal\n");

#if AUTO_ARBITRATION_REASONABLE_TEMP
		/*Thermal will issue "set parameter policy" than issue "register policy"*/
		Ap_temp = mtkts_btsmdpa_get_hw_temp();
		XTAL_temp = mtktsxtal_get_xtal_temp();
        printk("[ts_AP]Ap_temp=%d,XTAL_temp=%d,CPU_Tj=%d\n",Ap_temp,XTAL_temp,CPU_Tj);


        if(XTAL_temp > Ap_temp)
			AP_XTAL_diff = XTAL_temp - Ap_temp;
        else
            AP_XTAL_diff = Ap_temp - XTAL_temp;

        //check temp from Tj and Txal
        if(( Ap_temp < CPU_Tj) && (AP_XTAL_diff <= XTAL_BTSMDPA_TEMP_DIFF)){
            //printk("AP_XTAL_diff <= 10 degree\n");
			mtkts_btsmdpa_register_thermal();
		}
#else
		mtkts_btsmdpa_register_thermal();
#endif
		//AP_write_flag=1;
		return count;
	}
	else
	{
		mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_write] bad argument\n");
	}

	return -EINVAL;
}


void mtkts_btsmdpa_copy_table(BTSMDPA_TEMPERATURE *des,BTSMDPA_TEMPERATURE *src)
{
	int i=0;
    int j=0;

    j = (sizeof(BTSMDPA_Temperature_Table)/sizeof(BTSMDPA_TEMPERATURE));
	//xlog_printk(ANDROID_LOG_INFO, "Power/BTSMDPA_Thermal", "mtkts_btsmdpa_copy_table() : j = %d\n",j);
    for(i=0;i<j;i++)
	{
		des[i] = src[i];
	}
}

void mtkts_btsmdpa_prepare_table(int table_num)
{

	switch(table_num)
    {
		case 1://AP_NTC_BL197
				mtkts_btsmdpa_copy_table(BTSMDPA_Temperature_Table,BTSMDPA_Temperature_Table1);
                BUG_ON(sizeof(BTSMDPA_Temperature_Table)!=sizeof(BTSMDPA_Temperature_Table1));
			break;
		case 2://AP_NTC_TSM_1
                mtkts_btsmdpa_copy_table(BTSMDPA_Temperature_Table,BTSMDPA_Temperature_Table2);
                BUG_ON(sizeof(BTSMDPA_Temperature_Table)!=sizeof(BTSMDPA_Temperature_Table2));
			break;
		case 3://AP_NTC_10_SEN_1
                mtkts_btsmdpa_copy_table(BTSMDPA_Temperature_Table,BTSMDPA_Temperature_Table3);
                BUG_ON(sizeof(BTSMDPA_Temperature_Table)!=sizeof(BTSMDPA_Temperature_Table3));
			break;
		case 4://AP_NTC_10
                mtkts_btsmdpa_copy_table(BTSMDPA_Temperature_Table,BTSMDPA_Temperature_Table4);
                BUG_ON(sizeof(BTSMDPA_Temperature_Table)!=sizeof(BTSMDPA_Temperature_Table4));
			break;
		case 5://AP_NTC_47
                mtkts_btsmdpa_copy_table(BTSMDPA_Temperature_Table,BTSMDPA_Temperature_Table5);
                BUG_ON(sizeof(BTSMDPA_Temperature_Table)!=sizeof(BTSMDPA_Temperature_Table5));
			break;
		case 6://NTCG104EF104F
                mtkts_btsmdpa_copy_table(BTSMDPA_Temperature_Table,BTSMDPA_Temperature_Table6);
                BUG_ON(sizeof(BTSMDPA_Temperature_Table)!=sizeof(BTSMDPA_Temperature_Table6));
			break;
        default://AP_NTC_10
	            mtkts_btsmdpa_copy_table(BTSMDPA_Temperature_Table,BTSMDPA_Temperature_Table4);
                BUG_ON(sizeof(BTSMDPA_Temperature_Table)!=sizeof(BTSMDPA_Temperature_Table4));
            break;
    }

#if 0
{
	int i=0;
	for(i=0;i<(sizeof(BTSMDPA_Temperature_Table)/sizeof(BTSMDPA_TEMPERATURE));i++)
	{
		mtkts_btsmdpa_dprintk("BTSMDPA_Temperature_Table[%d].APteryTemp =%d\n",i, BTSMDPA_Temperature_Table[i].BTSMDPA_Temp);
		mtkts_btsmdpa_dprintk("BTSMDPA_Temperature_Table[%d].TemperatureR=%d\n",i, BTSMDPA_Temperature_Table[i].TemperatureR);
	}
}
#endif
}

static int mtkts_btsmdpa_param_read(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n",g_RAP_pull_up_R);
    seq_printf(m, "%d\n",g_RAP_pull_up_voltage);
    seq_printf(m, "%d\n",g_TAP_over_critical_low);
    seq_printf(m, "%d\n",g_RAP_ntc_table);
    seq_printf(m, "%d\n",g_RAP_ADC_channel);

	return 0;
}


static ssize_t mtkts_btsmdpa_param_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len=0;
	char desc[512];

    char pull_R[10],pull_V[10];
    char overcrilow[16];
    char NTC_TABLE[10];
    unsigned int valR,valV,over_cri_low,ntc_table;
    //external pin: 0/1/12/13/14/15, can't use pin:2/3/4/5/6/7/8/9/10/11,choose "adc_channel=11" to check if there is any param input
    unsigned int adc_channel=11;


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
	{
		return 0;
	}
	desc[len] = '\0';


	mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_write]\n");



    if (sscanf(desc, "%s %d %s %d %s %d %s %d %d",pull_R, &valR, pull_V, &valV, overcrilow, &over_cri_low,NTC_TABLE,&ntc_table,&adc_channel) >= 8)
	{

        if (!strcmp(pull_R, "PUP_R")) {
            g_RAP_pull_up_R = valR;
            mtkts_btsmdpa_dprintk("g_RAP_pull_up_R=%d\n",g_RAP_pull_up_R);
        }else{
			printk("[mtkts_btsmdpa_write] bad PUP_R argument\n");
            return -EINVAL;
        }

        if (!strcmp(pull_V, "PUP_VOLT")) {
            g_RAP_pull_up_voltage = valV;
            mtkts_btsmdpa_dprintk("g_Rat_pull_up_voltage=%d\n",g_RAP_pull_up_voltage);
        }else{
			printk("[mtkts_btsmdpa_write] bad PUP_VOLT argument\n");
            return -EINVAL;
        }

        if (!strcmp(overcrilow, "OVER_CRITICAL_L")) {
            g_TAP_over_critical_low = over_cri_low;
            mtkts_btsmdpa_dprintk("g_TAP_over_critical_low=%d\n",g_TAP_over_critical_low);
        }else{
			printk("[mtkts_btsmdpa_write] bad OVERCRIT_L argument\n");
            return -EINVAL;
        }

        if (!strcmp(NTC_TABLE, "NTC_TABLE")) {
            g_RAP_ntc_table = ntc_table;
            mtkts_btsmdpa_dprintk("g_RAP_ntc_table=%d\n",g_RAP_ntc_table);
        }else{
			printk("[mtkts_btsmdpa_write] bad NTC_TABLE argument\n");
            return -EINVAL;
        }

	    //external pin: 0/1/12/13/14/15, can't use pin:2/3/4/5/6/7/8/9/10/11,choose "adc_channel=11" to check if there is any param input
        if((adc_channel >= 2) && (adc_channel <= 11))
            g_RAP_ADC_channel = AUX_IN12_NTC;//check unsupport pin value, if unsupport, set channel = 1 as default setting.
		else{
			if(adc_channel!=11){//check if there is any param input, if not using default g_RAP_ADC_channel:1
				g_RAP_ADC_channel = adc_channel;
			}
	        else{
	            g_RAP_ADC_channel = AUX_IN12_NTC;
	        }
		}
        mtkts_btsmdpa_dprintk("adc_channel=%d\n",adc_channel);
        mtkts_btsmdpa_dprintk("g_RAP_ADC_channel=%d\n",g_RAP_ADC_channel);

		mtkts_btsmdpa_prepare_table(g_RAP_ntc_table);

		return count;
	}
	else
	{
		printk("[mtkts_btsmdpa_write] bad argument\n");
	}


	return -EINVAL;
}

//int  mtkts_btsmdpa_register_cooler(void)
//{
	/* cooling devices */
	//cl_dev_sysrst = mtk_thermal_cooling_device_register("mtktsbtsmdpatery-sysrst", NULL,
	//	&mtkts_btsmdpa_cooling_sysrst_ops);
	//return 0;
//}

int mtkts_btsmdpa_register_thermal(void)
{
	mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_register_thermal] \n");

    /* trips : trip 0~1 */
    if (NULL == thz_dev) {
        thz_dev = mtk_thermal_zone_device_register("mtktsbtsmdpa", num_trip, NULL,
                                                   &mtkts_btsmdpa_dev_ops, 0, 0, 0, interval*1000);
    }

	return 0;
}

//void mtkts_btsmdpa_unregister_cooler(void)
//{
	//if (cl_dev_sysrst) {
	//	mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
	//	cl_dev_sysrst = NULL;
	//}
//}
void mtkts_btsmdpa_unregister_thermal(void)
{
	mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_unregister_thermal] \n");

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtkts_btsmdpa_open(struct inode *inode, struct file *file)
{
    return single_open(file, mtkts_btsmdpa_read, NULL);
}
static const struct file_operations mtkts_btsmdpa_fops = {
    .owner = THIS_MODULE,
    .open = mtkts_btsmdpa_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mtkts_btsmdpa_write,
    .release = single_release,
};


static int mtkts_btsmdpa_param_open(struct inode *inode, struct file *file)
{
    return single_open(file, mtkts_btsmdpa_param_read, NULL);
}
static const struct file_operations mtkts_btsmdpa_param_fops = {
    .owner = THIS_MODULE,
    .open = mtkts_btsmdpa_param_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = mtkts_btsmdpa_param_write,
    .release = single_release,
};

static int __init mtkts_btsmdpa_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtkts_btsmdpa_dir = NULL;

	mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_init] \n");

    // setup default table
    mtkts_btsmdpa_prepare_table(g_RAP_ntc_table);

	mtkts_btsmdpa_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtkts_btsmdpa_dir)
	{
		mtkts_btsmdpa_dprintk("[%s]: mkdir /proc/driver/thermal failed\n", __func__);
	}
	else
	{
		entry = proc_create("tzbtspa", S_IRUGO | S_IWUSR | S_IWGRP, mtkts_btsmdpa_dir, &mtkts_btsmdpa_fops);
		if (entry) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
            proc_set_user(entry, 0, 1000);
#else // kernel ver
            entry->gid = 1000;
#endif // kernel ver
		}

		entry = proc_create("tzbtspa_param", S_IRUGO | S_IWUSR | S_IWGRP, mtkts_btsmdpa_dir, &mtkts_btsmdpa_param_fops);
		if (entry) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
            proc_set_user(entry, 0, 1000);
#else // kernel ver
            entry->gid = 1000;
#endif // kernel ver
		}

	}

	return 0;

	//mtkts_btsmdpa_unregister_cooler();
	return err;
}

static void __exit mtkts_btsmdpa_exit(void)
{
	mtkts_btsmdpa_dprintk("[mtkts_btsmdpa_exit] \n");
	mtkts_btsmdpa_unregister_thermal();
	//mtkts_btsmdpa_unregister_cooler();
}

#if AUTO_ARBITRATION_REASONABLE_TEMP
late_initcall(mtkts_btsmdpa_init);
module_exit(mtkts_btsmdpa_exit);
#else
module_init(mtkts_btsmdpa_init);
module_exit(mtkts_btsmdpa_exit);
#endif

