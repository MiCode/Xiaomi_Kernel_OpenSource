/*
 * Gas_Gauge driver for CW2015/2013
 * Copyright (C) 2012, CellWise
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * Authors: ChenGang <ben.chen@cellwise-semi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.And this driver depends on 
 * I2C and uses IIC bus for communication with the host.
 *
 */
#define BAT_CHANGE_ALGORITHM
// #define DEBUG    1

#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <mach/mt_gpio.h>
#include <linux/delay.h>
//#include <linux/power/cw2015_battery.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <mach/board.h>
#include <mach/cw2015_battery.h>
//#include "cw2015_battery.h"
#ifdef BAT_CHANGE_ALGORITHM
  #include <linux/fs.h>
  #include <linux/string.h>
  #include <linux/mm.h>
  #include <linux/syscalls.h>
  #include <asm/unistd.h>
  #include <asm/uaccess.h>
  #include <asm/fcntl.h>
  #define FILE_PATH "/data/lastsoc"
  //#define FILE_PATH "/dev/block/platform/mtk-msdc.0/by-name/proinfo"
  //#define FILE_PATH "/lastsoc"
  #define CPSOC  90
#endif
#include <linux/dev_info.h>//add liuchao
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/init.h>
//#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/input.h>
#include <linux/ioctl.h>
  #include <linux/fcntl.h>
#include <cust_charging.h>
#include <mach/charging.h>
//#include <mach/cw2015_battery.h>
//#include <linux/miscdevice.h>
//#include <linux/uaccess.h>
//#include <mach/mt_typedefs.h>


#define REG_VERSION             0x0
#define REG_VCELL               0x2
#define REG_SOC                 0x4
#define REG_RRT_ALERT           0x6
#define REG_CONFIG              0x8
#define REG_MODE                0xA
#define REG_BATINFO             0x10
#define MODE_SLEEP_MASK         (0x3<<6)
#define MODE_SLEEP              (0x3<<6)
#define MODE_NORMAL             (0x0<<6)
#define MODE_QUICK_START        (0x3<<4)
#define MODE_RESTART            (0xf<<0)

#define CONFIG_UPDATE_FLG       (0x1<<1)
#define ATHD                    (0x0<<3)        //ATHD = 0%

#define CW_I2C_SPEED            100000          // default i2c speed set 100khz
#define BATTERY_UP_MAX_CHANGE   420             // the max time allow battery change quantity
#define BATTERY_DOWN_CHANGE   60                // the max time allow battery change quantity
#define BATTERY_DOWN_MIN_CHANGE_RUN 30          // the min time allow battery change quantity when run
#define BATTERY_DOWN_MIN_CHANGE_SLEEP 1800      // the min time allow battery change quantity when run 30min

#define BATTERY_DOWN_MAX_CHANGE_RUN_AC_ONLINE 1800
#define DEVICE_RUN_TIME_FIX_VALUE 40

#define NO_STANDARD_AC_BIG_CHARGE_MODE 1
// #define SYSTEM_SHUTDOWN_VOLTAGE  3400000        //set system shutdown voltage related in battery info.
#define BAT_LOW_INTERRUPT    1

#define USB_CHARGER_MODE        1
#define AC_CHARGER_MODE         2

static struct i2c_client *cw2015_i2c_client; /* global i2c_client to support ioctl */
static struct workqueue_struct *cw2015_workqueue;

#define FG_CW2015_DEBUG 1
#define FG_CW2015_TAG                  "[FG_CW2015]"
#ifdef FG_CW2015_DEBUG
#define FG_CW2015_FUN(f)               printk(KERN_ERR FG_CW2015_TAG"%s\n", __FUNCTION__)
#define FG_CW2015_ERR(fmt, args...)    printk(KERN_ERR FG_CW2015_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define FG_CW2015_LOG(fmt, args...)    printk(KERN_ERR FG_CW2015_TAG fmt, ##args)
#endif
#define CW2015_DEV_NAME     "CW2015"
static const struct i2c_device_id FG_CW2015_i2c_id[] = {{CW2015_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_FG_CW2015={ I2C_BOARD_INFO("CW2015", 0x62)};
static struct i2c_driver FG_CW2015_i2c_driver;
int g_cw2015_capacity = 0;
int g_cw2015_vol = 0;
int g_mtk_init_vol = -10;
extern int FG_charging_type ;
extern int FG_charging_status ;
int CW2015_test_init=0;

extern int Charger_enable_Flag; //add by longcheer_liml_2015_10_12

#define queue_delayed_work_time  8000

/*Chaman add for create sysfs start*/
static int file_sys_state = 1;
/*Chaman add for create sysfs end*/


/*
extern int dwc_otg_check_dpdm(void);
extern int get_gadget_connect_flag(void);
extern int dwc_vbus_status( void );
*/
#if 0
#if defined(RGK_CW2015_BATTERY_TRX_4P20)
static u8 config_info[SIZE_BATINFO] = {
0x15,0x7F,0x5F,0x5D,0x56,0x60,0x53,0x4A,0x4C,0x49,0x48,0x45,0x3E,0x42,0x3A,0x2C,
0x22,0x1A,0x14,0x0C,0x12,0x2A,0x48,0x53,0x1E,0x35,0x0B,0x85,0x45,0x69,0x60,0x76,
0x81,0x6A,0x68,0x70,0x3E,0x1C,0x80,0x4C,0x07,0x3F,0x52,0x87,0x8F,0x91,0x94,0x52,
0x82,0x8C,0x92,0x96,0x83,0xD9,0xFF,0xCB,0x2F,0x7D,0x72,0xA5,0xB5,0xC1,0xD5,0x09
};
#elif defined(RGK_CW2015_BATTERY_DG_4P35)
static u8 config_info[SIZE_BATINFO] = {
0x17,0x52,0x6A,0x6A,0x66,0x63,0x61,0x59,0x62,0x6F,0x42,0x55,0x57,0x5A,0x44,0x3B,
0x30,0x29,0x22,0x1A,0x1C,0x2E,0x4A,0x56,0x17,0x32,0x0B,0x85,0x37,0x57,0x84,0x93,
0xA5,0x94,0x96,0x8B,0x3D,0x19,0x68,0x46,0x0A,0x4C,0x52,0x87,0x8F,0x91,0x94,0x52,
0x82,0x8C,0x92,0x96,0x5C,0xE1,0xE8,0xCB,0x2F,0x7D,0x72,0xA5,0xB5,0xC1,0xD8,0x09
};
#else
static u8 config_info[SIZE_BATINFO] = {
0x15,0x7D,0x5A,0x59,0x51,0x52,0x56,0x4A,0x49,0x4C,0x50,0x4C,0x42,0x34,0x2F,0x2A,
0x23,0x21,0x23,0x2A,0x2B,0x30,0x3E,0x46,0x21,0x71,0x0C,0x29,0x45,0x6A,0x71,0x85,
0x8B,0x87,0x84,0x86,0x46,0x1D,0x46,0x22,0x1D,0x7D,0x52,0x87,0x8F,0x91,0x94,0x52,
0x82,0x8C,0x92,0x96,0x7E,0xB4,0xC3,0xCB,0x2F,0x7D,0x72,0xA5,0xB5,0xC1,0x46,0xAE
};
#endif
#endif
#ifdef CONFIG_CM865_MAINBOARD
static u8 config_info_cos[SIZE_BATINFO] = { //guangyu coslight 180mv
0x17,0xF6,0x6A,0x6A,0x6D,0x66,0x67,0x63,0x5E,0x63,0x60,0x54,0x5B,0x5A,0x49,0x41,
0x36,0x2E,0x2B,0x20,0x21,0x2E,0x41,0x4E,0x34,0x1D,0x0C,0xCD,0x2C,0x4C,0x4E,0x5D,
0x69,0x65,0x67,0x68,0x3D,0x1A,0x6B,0x40,0x03,0x2B,0x38,0x71,0x84,0x95,0x9F,0x09,
0x36,0x6D,0x96,0xA2,0x5E,0xB3,0xE0,0x70,0x2F,0x7D,0x72,0xA5,0xB5,0xC1,0x46,0xAE
};
static u8 config_info_sun[SIZE_BATINFO] = {//xinwangda Sunwoda 600mv
0x17,0xEC,0x62,0x6B,0x6A,0x6B,0x67,0x64,0x60,0x63,0x60,0x56,0x5A,0x54,0x49,0x43,
0x36,0x31,0x2B,0x27,0x24,0x2E,0x43,0x4A,0x35,0x20,0x0C,0xCD,0x3C,0x5C,0x56,0x64,
0x6C,0x65,0x66,0x66,0x3E,0x1A,0x64,0x3D,0x04,0x2B,0x2D,0x52,0x83,0x96,0x98,0x13,
0x5F,0x8A,0x92,0xBF,0x46,0xA9,0xD9,0x70,0x2F,0x7D,0x72,0xA5,0xB5,0xC1,0x46,0xAE
};
static u8 config_info_scud[SIZE_BATINFO] = {//feimaotui Scud   900mV
0x17,0xF0,0x60,0x68,0x6C,0x6A,0x66,0x63,0x60,0x62,0x69,0x50,0x59,0x5B,0x4B,0x42,
0x3B,0x31,0x2B,0x24,0x20,0x32,0x49,0x59,0x17,0x17,0x0C,0xCD,0x2D,0x4D,0x53,0x62,
0x6D,0x60,0x5F,0x61,0x3C,0x1B,0x8E,0x2E,0x02,0x42,0x41,0x4F,0x84,0x96,0x96,0x2C,
0x4E,0x71,0x96,0xC1,0x4C,0xAC,0xE3,0xCB,0x2F,0x7D,0x72,0xA5,0xB5,0xC1,0x46,0xAE
};
#else
static u8 config_info_cos[SIZE_BATINFO] = {
0x17,0xF3,0x63,0x6A,0x6A,0x68,0x68,0x65,0x63,0x60,0x5B,0x59,0x65,0x5B,0x46,0x41,
0x36,0x31,0x28,0x27,0x31,0x35,0x43,0x51,0x1C,0x3B,0x0B,0x85,0x22,0x42,0x5B,0x82,
0x99,0x92,0x98,0x96,0x3D,0x1A,0x66,0x45,0x0B,0x29,0x52,0x87,0x8F,0x91,0x94,0x52,
0x82,0x8C,0x92,0x96,0x54,0xC2,0xBA,0xCB,0x2F,0x7D,0x72,0xA5,0xB5,0xC1,0xA5,0x49
};
static u8 config_info_des[SIZE_BATINFO] = { //desay
0x17,0xF9,0x6D,0x6D,0x6B,0x67,0x65,0x64,0x58,0x6D,0x6D,0x48,0x57,0x5D,0x4A,0x43,
0x37,0x31,0x2B,0x20,0x24,0x35,0x44,0x55,0x20,0x37,0x0B,0x85,0x2A,0x4A,0x56,0x68,
0x74,0x6B,0x6D,0x6E,0x3C,0x1A,0x5C,0x45,0x0B,0x30,0x52,0x87,0x8F,0x91,0x94,0x52,
0x82,0x8C,0x92,0x96,0x64,0xB4,0xDB,0xCB,0x2F,0x7D,0x72,0xA5,0xB5,0xC1,0xA5,0x42
};
#endif

#ifdef CONFIG_CM865_MAINBOARD
extern int PMIC_IMM_GetOneChannelValue(int dwChannel,int deCount,int trimd);
int hmi_battery_version=0;
void hmi_get_battery_version(void)
{
	int id_volt=0;
	id_volt=PMIC_IMM_GetOneChannelValue(BATTERY_ID_CHANNEL_NUM_PMIC,5,0);	
	printk("[fgauge_get_profile_id]id_vol id_volt= %d\n",id_volt);
	
	if(id_volt !=0)
	{
		if(id_volt < BATTERY_ID_VOLTAGE)
		{
			hmi_battery_version =1;
		}else if(id_volt < BATTERY_ID_VOLTAGE_2) 
		{
			hmi_battery_version =2;
		}else
		{
			hmi_battery_version =3;
		}
	}else{
			hmi_battery_version =0;
	}
	printk("~~liml_test_hmi_battery_version=%d\n",hmi_battery_version);
}

#else//def BATTERY_SWICTH
int hmi_battery_version=0;
void hmi_get_battery_version(void)
{
	char *ptr;
	ptr =strstr(saved_command_line,"batversion=");
	ptr +=strlen("batversion=");
	hmi_battery_version=simple_strtol(ptr,NULL,10);
	//bat_id=battery_ub_vendor();
	printk("liuchao_test_hmi_battery_version=%d\n",hmi_battery_version);
}
#endif

static struct cw_bat_platform_data cw_bat_platdata = {
	.dc_det_pin      = 0,
        .dc_det_level    = 0,

        .bat_low_pin    = 0,
        .bat_low_level  = 0,   
        .chg_ok_pin   = 0,
        .chg_ok_level = 0,

        .is_usb_charge = 0,
        .chg_mode_sel_pin = 0,
        .chg_mode_sel_level = 0,

        .cw_bat_config_info     = config_info_cos,
 
};

struct cw_battery {
        struct i2c_client *client;
        struct workqueue_struct *battery_workqueue;
        struct delayed_work battery_delay_work;
        struct delayed_work dc_wakeup_work;
        struct delayed_work bat_low_wakeup_work;
        struct cw_bat_platform_data *plat_data;

        struct power_supply rk_bat;
        struct power_supply rk_ac;
        struct power_supply rk_usb;

        long sleep_time_capacity_change;      // the sleep time from capacity change to present, it will set 0 when capacity change 
        long run_time_capacity_change;

        long sleep_time_charge_start;      // the sleep time from insert ac to present, it will set 0 when insert ac
        long run_time_charge_start;

        int dc_online;
        int usb_online;
        int charger_mode;
        int charger_init_mode;
        int capacity;
        int voltage;
        int status;
        int time_to_empty;
        int alt;

        int bat_change;
};

#ifdef BAT_CHANGE_ALGORITHM
struct cw_store{
        long bts; 
        int OldSOC;
        int DetSOC;
        int AlRunFlag;   
};
#endif

struct cw_battery *CW2015_obj = NULL;
static struct cw_battery *g_CW2015_ptr = NULL;

#ifdef BAT_CHANGE_ALGORITHM
  static int PowerResetFlag = -1;
  static int alg_run_flag = -1;
#endif

#ifdef BAT_CHANGE_ALGORITHM
static unsigned int cw_convertData(struct cw_battery *cw_bat,unsigned int ts)
{
    unsigned int i = ts%4096,n = ts/4096;
    unsigned int ret = 65536;

    if(i>=1700){i-=1700;ret=(ret*3)/4;}else{}
    if(i>=1700){i-=1700;ret=(ret*3)/4;}else{}
    if(i>=789){i-=789;ret=(ret*7)/8;}else{}
    if(i>=381){i-=381;ret=(ret*15)/16;}else{}  
    if(i>=188){i-=188;ret=(ret*31)/32;}else{}
    if(i>=188){i-=188;ret=(ret*31)/32;}else{}
    if(i>=93){i-=93;ret=(ret*61)/64;}else{}
    if(i>=46){i-=46;ret=(ret*127)/128;}else{}
    if(i>=23){i-=23;ret=(ret*255)/256;}else{}
    if(i>=11){i-=11;ret=(ret*511)/512;}else{}
    if(i>=6){i-=6;ret=(ret*1023)/1024;}else{}
    if(i>=3){i-=3;ret=(ret*2047)/2048;}else{} 
    if(i>=3){i-=3;ret=(ret*2047)/2048;}else{} 
    
    return ret>>n; 
}

static int AlgNeed(struct cw_battery *cw_bat, int SOC_NEW, int SOC_OLD)
{
	printk("Chaman num = %d SOC_NEW = %d   SOC_OLD = %d \n", __LINE__ ,SOC_NEW, SOC_OLD);
	if(SOC_NEW - SOC_OLD > -20 && SOC_NEW - SOC_OLD < 20){
		return 2; // this is old battery
	}else{
		return 1; // this is new battery
	}
}

static int cw_algorithm(struct cw_battery *cw_bat,int real_capacity)
{

    struct file *file = NULL;
    struct cw_store st;
    struct inode *inode;
    mm_segment_t old_fs;
    int fileresult;
    int vmSOC; 
    unsigned int utemp,utemp1;
    long timeNow;
  int count = 0;

	/*0728 Chaman start*/
	static unsigned long timeNow_last  = -1;
	long timeChanged = 0;
	long timeChanged_rtc = 0;
	struct timespec ts;
	/*0728 Chaman end*/
	
    static int count_fail=0;
	static int SOC_Dvalue = 0;
	static int Time_real3 = 0;
	static int Time_Dvalue = 0;
	static int Join_Fast_Close_SOC = 0;
       struct timespec ktime_ts;
       long suspend_time = 0;
       static unsigned long suspend_time_last  = 0;
       long suspend_time_changed = 0;
	static int count_time=0;
	static int mtk_init_vol = -10;

	/*Chaman add for create sysfs start*/
	static int return_vmSOC = 0;
	/*Chaman add for create sysfs end*/

	
#define USE_MTK_INIT_VOL
//#undef USE_MTK_INIT_VOL

	#ifdef USE_MTK_INIT_VOL
	if(mtk_init_vol == -10 && g_mtk_init_vol != -10){
		mtk_init_vol = g_mtk_init_vol;
		mtk_init_vol = mtk_init_vol - 65;
		printk("Chaman %s %d mtk_init_vol = %d !\n", __FILE__, __LINE__, mtk_init_vol);
	}
	if(mtk_init_vol == -10){
		printk("Chaman check mtk init soc is not be saved! why??\n");
		return real_capacity;
	}
	printk("Chaman %s %d mtk_init_vol = %d !\n", __FILE__, __LINE__, mtk_init_vol);
	#endif

    //FG_CW2015_ERR("cw2015_file_test sizeof(long) = %d sizeof(int) = %d sizeof(st) = %d\n",sizeof(long),sizeof(int),sizeof(st));
    timeNow = get_seconds();
    vmSOC = real_capacity;

file = filp_open(FILE_PATH,O_RDWR|O_CREAT,0644);

    if(IS_ERR(file))
    {
        FG_CW2015_ERR(" error occured while opening file %s,exiting...\n",FILE_PATH);
        return real_capacity;
    }
	
    old_fs = get_fs();
    set_fs(KERNEL_DS); 
    inode = file->f_dentry->d_inode;

    if((long)(inode->i_size)<(long)sizeof(st))//(inode->i_size)<sizeof(st)
    {
        if(count_fail < 2)
            {
              count_fail++;
		  	filp_close(file,NULL);
              return real_capacity;
            }
         
        st.bts = timeNow;
        st.OldSOC = real_capacity;
        st.DetSOC = 0;
        st.AlRunFlag = -1; 
        //alg_run_flag = -1;
        //file->f_pos = 0;  
        //vfs_write(file,(char*)&st,sizeof(st),&file->f_pos); 
        FG_CW2015_ERR("cw2015_file_test  file size error!\n");
    }
    else
    {
        count_fail=0;
        file->f_pos = 0;
        vfs_read(file,(char*)&st,sizeof(st),&file->f_pos);

        FG_CW2015_ERR(" success opening file, file_path=%s \n", FILE_PATH);
    }
	
	/*0909 start*/
	get_monotonic_boottime(&ts);
       ktime_get_ts(&ktime_ts);
       suspend_time = ts.tv_sec - ktime_ts.tv_sec;
       if(timeNow_last != -1 && ts.tv_sec > DEVICE_RUN_TIME_FIX_VALUE){
              suspend_time_changed = suspend_time - suspend_time_last;
		timeChanged_rtc = timeNow - timeNow_last;
               timeChanged = timeNow - timeNow_last -suspend_time_changed;
               printk(KERN_INFO "[FW_2015]suspend_time_changed = \t%ld,timeChanged_rtc = \t%ld, timeChanged = \t%ld\n",
                          suspend_time_changed, timeChanged_rtc, timeChanged);
		if(timeChanged < -60 || timeChanged > 60){
		st.bts = st.bts + timeChanged_rtc;
              FG_CW2015_ERR(" 1 st.bts = \t%ld\n", st.bts);
		}
	}   
	timeNow_last = timeNow;
       suspend_time_last = suspend_time;


	if(((st.bts) < 0) || (st.OldSOC > 100) || (st.OldSOC < 0) || (st.DetSOC < -1)) 
	/*0728 Chaman end*/
    {
        FG_CW2015_ERR("cw2015_file_test  reading file error!\n"); 
        FG_CW2015_ERR("cw2015_file_test  st.bts = %ld st.OldSOC = %d st.DetSOC = %d st.AlRunFlag = %d  vmSOC = %d  2015SOC=%d\n",st.bts,st.OldSOC,st.DetSOC,st.AlRunFlag,vmSOC,real_capacity); 
  
        st.bts = timeNow;
        st.OldSOC = real_capacity;
        st.DetSOC = 0;
        st.AlRunFlag = -1; 
        //FG_CW2015_ERR("cw2015_file_test  reading file error!\n");
    }
	
	/*Chaman 0729 need check start*/
    if(PowerResetFlag == 1)
    {
        PowerResetFlag = -1;
		#ifdef USE_MTK_INIT_VOL
		if(mtk_init_vol > 4372){
			st.DetSOC = 100 - real_capacity;
		}else if(mtk_init_vol > 4349){
			st.DetSOC = 98 - real_capacity;
		}else if(mtk_init_vol > 4325){
			st.DetSOC = 96 - real_capacity;
		}else if(mtk_init_vol > 4302){
			st.DetSOC = 94 - real_capacity;
		}else if(mtk_init_vol > 4278){
			st.DetSOC = 92 - real_capacity;
		}else if(mtk_init_vol > 4255){
			st.DetSOC = 90 - real_capacity;
		}else if(mtk_init_vol > 4233){
			st.DetSOC = 88 - real_capacity;
		}else if(mtk_init_vol > 4211){
			st.DetSOC = 86 - real_capacity;
		}else if(mtk_init_vol > 4189){
			st.DetSOC = 84 - real_capacity;
		}else if(mtk_init_vol > 4168){
			st.DetSOC = 82 - real_capacity;
		}else if(mtk_init_vol > 4147){
			st.DetSOC = 80 - real_capacity;
		}else if(mtk_init_vol > 4126){
			st.DetSOC = 78 - real_capacity;
		}else if(mtk_init_vol > 4106){
			st.DetSOC = 76 - real_capacity;
		}else if(mtk_init_vol > 4089){
			st.DetSOC = 74 - real_capacity;
		}else if(mtk_init_vol > 4071){
			st.DetSOC = 72 - real_capacity;
		}else if(mtk_init_vol > 4048){
			st.DetSOC = 70 - real_capacity;
		}else if(mtk_init_vol > 4024){
			st.DetSOC = 68 - real_capacity;
		}else if(mtk_init_vol > 4001){
			st.DetSOC = 66 - real_capacity;
		}else if(mtk_init_vol > 3977){
			st.DetSOC = 64 - real_capacity;
		}else if(mtk_init_vol > 3965){
			st.DetSOC = 62 - real_capacity;
		}else if(mtk_init_vol > 3953){
			st.DetSOC = 60 - real_capacity;
		}else if(mtk_init_vol > 3936){
			st.DetSOC = 58 - real_capacity;
		}else if(mtk_init_vol > 3919){
			st.DetSOC = 56 - real_capacity;
		}else if(mtk_init_vol > 3901){
			st.DetSOC = 54 - real_capacity;
		}else if(mtk_init_vol > 3882){
			st.DetSOC = 52 - real_capacity;
		}else if(mtk_init_vol > 3869){
			st.DetSOC = 50 - real_capacity;
		}else if(mtk_init_vol > 3857){
			st.DetSOC = 48 - real_capacity;
		}else if(mtk_init_vol > 3846){
			st.DetSOC = 46 - real_capacity;
		}else if(mtk_init_vol > 3835){
			st.DetSOC = 44 - real_capacity;
		}else if(mtk_init_vol > 3827){
			st.DetSOC = 42 - real_capacity;
		}else if(mtk_init_vol > 3818){
			st.DetSOC = 40 - real_capacity;
		}else if(mtk_init_vol > 3811){
			st.DetSOC = 38 - real_capacity;
		}else if(mtk_init_vol > 3804){
			st.DetSOC = 36 - real_capacity;
		}else if(mtk_init_vol > 3797){
			st.DetSOC = 34 - real_capacity;
		}else if(mtk_init_vol > 3790){
			st.DetSOC = 32 - real_capacity;
		}else if(mtk_init_vol > 3786){
			st.DetSOC = 30 - real_capacity;
		}else if(mtk_init_vol > 3781){
			st.DetSOC = 28 - real_capacity;
		}else if(mtk_init_vol > 3775){
			st.DetSOC = 26 - real_capacity;
		}else if(mtk_init_vol > 3770){
			st.DetSOC = 24 - real_capacity;
		}else if(mtk_init_vol > 3762){
			st.DetSOC = 22 - real_capacity;
		}else if(mtk_init_vol > 3753){
			st.DetSOC = 20 - real_capacity;
		}else if(mtk_init_vol > 3742){
			st.DetSOC = 18 - real_capacity;
		}else if(mtk_init_vol > 3731){
			st.DetSOC = 16 - real_capacity;
		}else if(mtk_init_vol > 3715){
			st.DetSOC = 14 - real_capacity;
		}else if(mtk_init_vol > 3699){
			st.DetSOC = 12 - real_capacity;
		}else if(mtk_init_vol > 3694){
			st.DetSOC = 10 - real_capacity;
		}else if(mtk_init_vol > 3689){
			st.DetSOC = 8 - real_capacity;
		}else if(mtk_init_vol > 3681){
			st.DetSOC = 6 - real_capacity;
		}else if(mtk_init_vol > 3673){
			st.DetSOC = 4 - real_capacity;
		}else if(mtk_init_vol > 3660){
			st.DetSOC = 3 - real_capacity;
		}else{
			st.DetSOC = 1;
		}
		#else 
if(hmi_battery_version==2){
		if(real_capacity == 0){
			st.DetSOC = 1;
		}else if(real_capacity == 1){
			st.DetSOC = 3;
		}else if(real_capacity == 2){
			st.DetSOC = 10;
		}else if(real_capacity == 3){
			st.DetSOC = 19;
		}else if(real_capacity == 4){
			st.DetSOC = 20;
		}else if(real_capacity == 5){
			st.DetSOC = 22;
		}else if(real_capacity < 14){
			st.DetSOC = 23;
		}else if(real_capacity < 21){
			st.DetSOC = 26;
		}else if(real_capacity < 26){
			st.DetSOC = 25;
		}else if(real_capacity < 31){
			st.DetSOC = 23;
		}else if(real_capacity < 36){
			st.DetSOC = 22;
		}else if(real_capacity < 41){
			st.DetSOC = 20;
		}else if(real_capacity < 51){
			st.DetSOC = 16;
		}else if(real_capacity < 61){
			st.DetSOC = 9;
		}else if(real_capacity < 71){
			st.DetSOC = 7;
		}else if(real_capacity < 81){
			st.DetSOC = 8;
		}else if(real_capacity < 88){
			st.DetSOC = 9;
		}else if(real_capacity < 94){
			st.DetSOC = 6;
		}else if(real_capacity <= 100){
            vmSOC = 100;
            st.DetSOC = 100 - real_capacity;
		}
}else{
		if(real_capacity == 0){
			st.DetSOC = 1;
		}else if(real_capacity == 1){
			st.DetSOC = 3;
		}else if(real_capacity == 2){
			st.DetSOC = 10;
		}else if(real_capacity == 3){
			st.DetSOC = 20;
		}else if(real_capacity == 4){
			st.DetSOC = 23;
		}else if(real_capacity == 5){
			st.DetSOC = 27;
		}else if(real_capacity < 14){
			st.DetSOC = 28;
		}else if(real_capacity < 21){
			st.DetSOC = 30;
		}else if(real_capacity < 26){
			st.DetSOC = 25;
		}else if(real_capacity < 34){
			st.DetSOC = 20;
		}else if(real_capacity < 39){
			st.DetSOC = 17;
		}else if(real_capacity < 45){
			st.DetSOC = 13;
		}else if(real_capacity < 51){
			st.DetSOC = 12;
		}else if(real_capacity < 61){
			st.DetSOC = 11;
		}else if(real_capacity < 71){
			st.DetSOC = 13;
		}else if(real_capacity < 81){
			st.DetSOC = 11;
		}else if(real_capacity < 90){
			st.DetSOC = 10;
		}else if(real_capacity <= 100){
            vmSOC = 100;
            st.DetSOC = 100 - real_capacity;
		}
}

		#endif

		if(AlgNeed(cw_bat, st.DetSOC + real_capacity, st.OldSOC) == 2){
			st.DetSOC = st.OldSOC - real_capacity + 1;
        	FG_CW2015_ERR("st.DetDoc=%d\n", st.DetSOC);
		}

        st.AlRunFlag = 1;
        st.bts = timeNow;
		vmSOC = real_capacity + st.DetSOC;
        FG_CW2015_ERR("cw2015_file_test  PowerResetFlag == 1!\n");
    }

	/*Chaman 0729 need check end*/
	
	else if(Join_Fast_Close_SOC && (st.AlRunFlag > 0)){
		if(timeNow >= (Time_real3 + Time_Dvalue)){
			vmSOC = st.OldSOC - 1;
			Time_real3 = timeNow;
		}
		/*0728 Chaman start*/
		else{
			vmSOC = st.OldSOC;
		}
		/*0728 Chaman end*/
		if (vmSOC == real_capacity)
		{ 
			st.AlRunFlag = -1;
			FG_CW2015_ERR("cw2015_file_test  algriothm end of decrease acceleration\n");
		}

	}
    else  if(((st.AlRunFlag) >0)&&((st.DetSOC) != 0))
    {
        /*caculation */
		/*0728 Chaman start */
		get_monotonic_boottime(&ts);
		if(real_capacity < 1 && cw_bat->charger_mode == 0 && ts.tv_sec > DEVICE_RUN_TIME_FIX_VALUE){//add 0702 for vmSOC to real_capacity quickly when real_capacity very low
			if (SOC_Dvalue == 0){
				SOC_Dvalue = st.OldSOC - real_capacity;
                             if(SOC_Dvalue == 0)
                             {
                                 st.AlRunFlag = -1;
                                 printk(KERN_INFO "[FG_CW2015]cw2015_file_test  algriothm end of decrease acceleration[2]\n");
                             }
                             else
                             {
                                 printk(KERN_INFO "[FG_CW2015]cw2015_file_test  begin of decrease acceleration \n");
				    Time_real3 = timeNow;
                                 if((cw_bat->voltage) < 3480){
				        Time_Dvalue = 20/(SOC_Dvalue);
                                 }
                                 else{
                                     Time_Dvalue = 90/(SOC_Dvalue);
                                 }
				    Join_Fast_Close_SOC = 1;
				    vmSOC = st.OldSOC;
                             }
			}			
		}/*0728 Chaman end*/
		else
		{
			utemp1 = 32768/(st.DetSOC);
	        if((st.bts)<timeNow)
	            utemp = cw_convertData(cw_bat,(timeNow-st.bts));   
	        else
	            utemp = cw_convertData(cw_bat,1); 
	        FG_CW2015_ERR("cw2015_file_test  convertdata = %d\n",utemp);
	        if((st.DetSOC)<0)
	            vmSOC = real_capacity-(int)((((unsigned int)((st.DetSOC)*(-1))*utemp)+utemp1)/65536);
	        else
	            vmSOC = real_capacity+(int)((((unsigned int)(st.DetSOC)*utemp)+utemp1)/65536);

		    if (vmSOC == real_capacity)
		    { 
		        st.AlRunFlag = -1;
		        FG_CW2015_ERR("cw2015_file_test  algriothm end\n");
		    }
		}
    }
    else
    {
        /*Game over*/
        st.AlRunFlag = -1;
        st.bts = timeNow;
        FG_CW2015_ERR("cw2015_file_test  no algriothm\n");
    }
    //FG_CW2015_ERR("cw2015_file_test  sizeof(st) = %d filesize = %ld time = %d\n ",sizeof(st),(long)(inode->i_size),timeNow);
    //FG_CW2015_ERR("cw2015_file_test  st.bts = %ld st.OldSOC = %d st.DetSOC = %d st.AlRunFlag = %d  vmSOC = %d  2015SOC=%d\n",st.bts,st.OldSOC,st.DetSOC,st.AlRunFlag,vmSOC,real_capacity);
    FG_CW2015_ERR("cw2015_file_test debugdata,\t%ld,\t%d,\t%d,\t%d,\t%d,\t%ld,\t%d,\t%d,\t%d\n",timeNow,cw_bat->capacity,cw_bat->voltage,vmSOC,st.DetSOC,st.bts,st.AlRunFlag,real_capacity,st.OldSOC);
    alg_run_flag = st.AlRunFlag;
    if(vmSOC>100)
        vmSOC = 100;
    else if(vmSOC<0)    
        vmSOC = 0;
    st.OldSOC = vmSOC;
    file->f_pos = 0;
    vfs_write(file,(char*)&st,sizeof(st),&file->f_pos);
    set_fs(old_fs);
    filp_close(file,NULL);
    file = NULL;

	/*Chaman add for create sysfs start*/
	if(return_vmSOC < 5){
		if(return_vmSOC > 0){
			file_sys_state = 2;
			printk("cw2015 return_vmsoc=%d, file_sys_state=%d \n", return_vmSOC, file_sys_state);
		}
		return_vmSOC++;
	}
/*Chaman add for create sysfs end*/
    return vmSOC;

}
#endif


static int cw_read(struct i2c_client *client, u8 reg, u8 buf[])
{
        int ret = 0;
#if 1
	ret = i2c_smbus_read_byte_data(client,reg);
	printk("cw_read buf2 = %d",ret);
	if (ret < 0)
	{
        return ret;
	}
	else
	{
	    buf[0] = ret;
	    ret = 0;
	}
#else
        ret = i2c_master_reg8_recv(client, reg, buf, 1, CW_I2C_SPEED);
#endif
        return ret;
}

static int cw_write(struct i2c_client *client, u8 reg, u8 const buf[])
{
        int ret = 0;
#if 1
	ret =  i2c_smbus_write_byte_data(client,reg,buf[0]);
#else
        ret = i2c_master_reg8_send(client, reg, buf, 1, CW_I2C_SPEED);
#endif
        return ret;
}

static int cw_read_word(struct i2c_client *client, u8 reg, u8 buf[])
{
        int ret = 0;
	 unsigned int data = 0;
#if 1
        data = i2c_smbus_read_word_data(client, reg);
	 buf[0] = data & 0x00FF;
	 buf[1] = (data & 0xFF00)>>8;
#else
        ret = i2c_master_reg8_recv(client, reg, buf, 2, CW_I2C_SPEED);
#endif
        return ret;
}

static int cw_update_config_info(struct cw_battery *cw_bat)
{
        int ret;
        u8 reg_val;
        int i;
        u8 reset_val;
#ifdef FG_CW2015_DEBUG
		FG_CW2015_LOG("func: %s-------\n", __func__);
#else
        //FG_CW2015_ERR("func: %s-------\n", __func__);
#endif  
#ifdef CONFIG_CM865_MAINBOARD     
	if(hmi_battery_version==2)
	{
		FG_CW2015_LOG("test cw_bat_config_info = 0x%x",config_info_sun[0]);
	}else if(hmi_battery_version==3)
	{
		FG_CW2015_LOG("test cw_bat_config_info = 0x%x",config_info_scud[0]);
	}else{
		FG_CW2015_LOG("test cw_bat_config_info = 0x%x",config_info_cos[0]);
	}      
#else
	if(hmi_battery_version==2)
        FG_CW2015_LOG("test cw_bat_config_info = 0x%x",config_info_des[0]);//liuchao
	else
        FG_CW2015_LOG("test cw_bat_config_info = 0x%x",config_info_cos[0]);//liuchao
#endif
        /* make sure no in sleep mode */
        ret = cw_read(cw_bat->client, REG_MODE, &reg_val);
	FG_CW2015_LOG("cw_update_config_info reg_val = 0x%x",reg_val);
        if (ret < 0)
                return ret;

        reset_val = reg_val;
        if((reg_val & MODE_SLEEP_MASK) == MODE_SLEEP) {
#ifdef FG_CW2015_DEBUG
				  FG_CW2015_ERR("Error, device in sleep mode, cannot update battery info\n");
#else
                //dev_err(&cw_bat->client->dev, "Error, device in sleep mode, cannot update battery info\n");
#endif
                return -1;
        }

        /* update new battery info */
        for (i = 0; i < SIZE_BATINFO; i++) {
#ifdef FG_CW2015_DEBUG
                //FG_CW2015_LOG("cw_bat->plat_data->cw_bat_config_info[%d] = 0x%x\n", i, \
                //                cw_bat->plat_data->cw_bat_config_info[i]);
#else
                /*FG_CW2015_ERR("cw_bat->plat_data->cw_bat_config_info[%d] = 0x%x\n", i, \
                                cw_bat->plat_data->cw_bat_config_info[i]);*/
#endif

#ifdef CONFIG_CM865_MAINBOARD  
			if(hmi_battery_version==2)
			{
				ret = cw_write(cw_bat->client, REG_BATINFO + i, &config_info_sun[i]);
			}else if(hmi_battery_version==3)
			{
				ret = cw_write(cw_bat->client, REG_BATINFO + i, &config_info_scud[i]);
			}else{
				ret = cw_write(cw_bat->client, REG_BATINFO + i, &config_info_cos[i]);
			}
	    	if (ret < 0)
	    	{
	    		return ret;
			} 
                        
        }
	/* readback & check */
        for (i = 0; i < SIZE_BATINFO; i++)
        {
			ret = cw_read(cw_bat->client, REG_BATINFO + i, &reg_val);
			if(hmi_battery_version==2)
			{
                if (reg_val != config_info_sun[i])
                        return -1;
			}else if(hmi_battery_version==3)
			{
				if (reg_val != config_info_scud[i])
                        return -1;
			}else{
                if (reg_val != config_info_cos[i])
                        return -1;
			}
		
       }
#else
			if(hmi_battery_version==2)
			{
				ret = cw_write(cw_bat->client, REG_BATINFO + i, &config_info_des[i]);
			}else{
				ret = cw_write(cw_bat->client, REG_BATINFO + i, &config_info_cos[i]);
			}
	    	if (ret < 0)
	    	{
	    		return ret;
			} 
                        
        }
	/* readback & check */
        for (i = 0; i < SIZE_BATINFO; i++)
        {
			ret = cw_read(cw_bat->client, REG_BATINFO + i, &reg_val);
			if(hmi_battery_version==2)
			{
                if (reg_val != config_info_des[i])
                        return -1;
			}else{
                if (reg_val != config_info_cos[i])
                        return -1;
			}
		
       }
#endif

        
        /* set cw2015/cw2013 to use new battery info */
        ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
        if (ret < 0)
                return ret;

        reg_val |= CONFIG_UPDATE_FLG;   /* set UPDATE_FLAG */
        reg_val &= 0x07;                /* clear ATHD */
        reg_val |= ATHD;                /* set ATHD */
        ret = cw_write(cw_bat->client, REG_CONFIG, &reg_val);
        if (ret < 0)
                return ret;

        /* check 2015/cw2013 for ATHD & update_flag */ 
        ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
        if (ret < 0)
                return ret;
        
        if (!(reg_val & CONFIG_UPDATE_FLG)) {
#ifdef FG_CW2015_DEBUG
                FG_CW2015_LOG("update flag for new battery info have not set..\n");
#else
                //FG_CW2015_ERR("update flag for new battery info have not set..\n");
#endif
        }

        if ((reg_val & 0xf8) != ATHD) {
#ifdef FG_CW2015_DEBUG
                FG_CW2015_LOG("the new ATHD have not set..\n");
#else
                //FG_CW2015_ERR("the new ATHD have not set..\n");
#endif
        }

        /* reset */
        reset_val &= ~(MODE_RESTART);
        reg_val = reset_val | MODE_RESTART;
        ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
        if (ret < 0)
                return ret;

        msleep(10);
        ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
        if (ret < 0)
                return ret;
#ifdef  BAT_CHANGE_ALGORITHM
        PowerResetFlag = 1;
        FG_CW2015_ERR("cw2015_file_test  set PowerResetFlag/n ");
#endif
		msleep(10);
        
        return 0;
}

static int cw_init(struct cw_battery *cw_bat)
{
	int ret;
	int i;
	u8 reg_val = MODE_SLEEP;
	hmi_get_battery_version();//liuchao

	struct devinfo_struct *dev = (struct devinfo_struct*)kmalloc(sizeof(struct devinfo_struct), GFP_KERNEL);
	dev->device_type = "Battery";
	dev->device_vendor = DEVINFO_NULL;
	dev->device_ic = DEVINFO_NULL;
	dev->device_version = DEVINFO_NULL;
#ifdef CONFIG_CM865_MAINBOARD
	if(hmi_battery_version==1)		
	dev->device_module = "Cos"; 
	else if(hmi_battery_version==2)	
	dev->device_module = "Sun";
	else if(hmi_battery_version==3)	
	dev->device_module = "Scud";
#else
	if(hmi_battery_version==1)		
	dev->device_module = "Cos"; 
	else if(hmi_battery_version==2)	
	dev->device_module = "Des";
#endif
	else
	dev->device_module = "ERROR";	
	dev->device_info = DEVINFO_NULL;
	dev->device_used = DEVINFO_USED;	
	DEVINFO_CHECK_ADD_DEVICE(dev);


#if 0
	ret = cw_read(cw_bat->client, REG_MODE, &reg_val);
	if (ret < 0)
	{
		return ret;
	}

#endif
     //   printk("cw2015_init_-%d\n",__LINE__);
	if ((reg_val & MODE_SLEEP_MASK) == MODE_SLEEP) 
	{
		reg_val = MODE_NORMAL;
      	//   printk("cw2015_init_-%d\n",__LINE__);
		ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
		if (ret < 0)
		{
		 	return ret;
		} 
                       
	}
     //    printk("cw2015_init_-%d\n",__LINE__);
	ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
	{
		return ret;
	}
               
	//          printk("cw2015_init_-%d\n",__LINE__);
	FG_CW2015_LOG("the new ATHD have not set reg_val = 0x%x\n",reg_val);
	if ((reg_val & 0xf8) != ATHD) 
	{
#ifdef FG_CW2015_DEBUG
		FG_CW2015_LOG("the new ATHD have not set\n");
#else
		//FG_CW2015_ERR("the new ATHD have not set\n");
#endif
		reg_val &= 0x07;    /* clear ATHD */
		reg_val |= ATHD;    /* set ATHD */
		ret = cw_write(cw_bat->client, REG_CONFIG, &reg_val);
		FG_CW2015_LOG("cw_init 1111\n");
		if (ret < 0)
		{
			return ret;
		}
                       
 	}

	ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
	{
		 return ret;
	} 
               
	FG_CW2015_LOG("cw_init REG_CONFIG = %d\n",reg_val);
	
 	if (!(reg_val & CONFIG_UPDATE_FLG))
	{
#ifdef FG_CW2015_DEBUG
		FG_CW2015_LOG("update flag for new battery info have not set\n");
#else
		//FG_CW2015_ERR("update flag for new battery info have not set\n");
#endif

#ifdef CONFIG_CM865_MAINBOARD
		ret = cw_update_config_info(cw_bat);
		if (ret < 0)
		{
			return ret;
		}
	} else {
		for(i = 0; i < SIZE_BATINFO; i++) { 
			ret = cw_read(cw_bat->client, (REG_BATINFO + i), &reg_val);
			if (ret < 0)
			{
				return ret;
			}
                                
			if(hmi_battery_version==2)
			{
			 	if (config_info_sun[i] != reg_val)
					break;
			}else if(hmi_battery_version==3)
			{
				if (config_info_scud[i] != reg_val)
					break;
			}else
			{
				if (config_info_cos[i] != reg_val)
					break;
			}
		}
#else
		ret = cw_update_config_info(cw_bat);
		if (ret < 0)
		{
			return ret;
		}
                        
	} else {
		for(i = 0; i < SIZE_BATINFO; i++) 
		{ 
			ret = cw_read(cw_bat->client, (REG_BATINFO + i), &reg_val);
			if (ret < 0)
			{
				 return ret;
			}
                               
 			if(hmi_battery_version==2)
			{
			 	if (config_info_des[i] != reg_val)
					break;
			}else
			{
			 	if (config_info_cos[i] != reg_val)
				break;
			}
		}
#endif

		if (i != SIZE_BATINFO) {
#ifdef FG_CW2015_DEBUG
			FG_CW2015_LOG("update flag for new battery info have not set\n"); 
#else
			//FG_CW2015_ERR("update flag for new battery info have not set\n"); 
#endif
			ret = cw_update_config_info(cw_bat);
			if (ret < 0)
			{
				 return ret;
			}
                               
		}
	}

	for (i = 0; i < 30; i++) 
	{
		ret = cw_read(cw_bat->client, REG_SOC, &reg_val);
		if (ret < 0)
		{
			return ret;
		}else if (reg_val <= 0x64) 
		{
			break;
		}
		msleep(100);
		if (i > 25)
		{
#ifdef FG_CW2015_DEBUG
			FG_CW2015_ERR("cw2015/cw2013 input unvalid power error\n");
#else
			//dev_err(&cw_bat->client->dev, "cw2015/cw2013 input unvalid power error\n");
#endif
		}

	}
	if (i >=30)
	{
		reg_val = MODE_SLEEP;
		ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
#ifdef FG_CW2015_DEBUG
		FG_CW2015_ERR("cw2015/cw2013 input unvalid power error_2\n");
#else
		//dev_err(&cw_bat->client->dev, "cw2015/cw2013 input unvalid power error_2\n");
#endif
		return -1;
	} 
	CW2015_test_init=1;
	
	return 0;
}

static void cw_update_time_member_charge_start(struct cw_battery *cw_bat)
{
        struct timespec ts;
        int new_run_time;
        int new_sleep_time;

        ktime_get_ts(&ts);
        new_run_time = ts.tv_sec;

        get_monotonic_boottime(&ts);
        new_sleep_time = ts.tv_sec - new_run_time;

        cw_bat->run_time_charge_start = new_run_time;
        cw_bat->sleep_time_charge_start = new_sleep_time; 
}

static void cw_update_time_member_capacity_change(struct cw_battery *cw_bat)
{
        struct timespec ts;
        int new_run_time;
        int new_sleep_time;

        ktime_get_ts(&ts);
        new_run_time = ts.tv_sec;

        get_monotonic_boottime(&ts);
        new_sleep_time = ts.tv_sec - new_run_time;

        cw_bat->run_time_capacity_change = new_run_time;
        cw_bat->sleep_time_capacity_change = new_sleep_time; 
}
extern int g_platform_boot_mode;
static int cw_quickstart(struct cw_battery *cw_bat)
{
        int ret = 0;
        u8 reg_val = MODE_QUICK_START;

        ret = cw_write(cw_bat->client, REG_MODE, &reg_val);     //(MODE_QUICK_START | MODE_NORMAL));  // 0x30
        if(ret < 0) {
#ifdef FG_CW2015_DEBUG
                FG_CW2015_ERR("Error quick start1\n");
#else
               // dev_err(&cw_bat->client->dev, "Error quick start1\n");
#endif
                return ret;
        }
        
        reg_val = MODE_NORMAL;
        ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
        if(ret < 0) {
#ifdef FG_CW2015_DEBUG
                FG_CW2015_ERR("Error quick start2\n");
#else
                //dev_err(&cw_bat->client->dev, "Error quick start2\n");
#endif
                return ret;
        }
        return 1;
}

static int cw_get_capacity(struct cw_battery *cw_bat)
{
        int cw_capacity;
        int ret;
        u8 reg_val[2];

        struct timespec ts;
        long new_run_time;
        long new_sleep_time;
        long capacity_or_aconline_time;
        int allow_change;
        int allow_capacity;
        static int if_quickstart = 0;
        static int jump_flag =0;
        static int reset_loop =0;
        int charge_time;
        u8 reset_val;
        int loop =0;
        static int count_time=0;
        static int count_time_sum=0;
		static int count_real_capacity=0;
        
		u8 count_real_sum = 0;

	    	
        ret = cw_read_word(cw_bat->client, REG_SOC, reg_val);
        if (ret < 0)
                return ret;
	 FG_CW2015_LOG("cw_get_capacity cw_capacity_0 = %d,cw_capacity_1 = %d\n",reg_val[0],reg_val[1]);
        cw_capacity = reg_val[0];
        if ((cw_capacity < 0) || (cw_capacity > 100)) {
#ifdef FG_CW2015_DEBUG
                FG_CW2015_ERR("get cw_capacity error; cw_capacity = %d\n", cw_capacity);
#else
                //dev_err(&cw_bat->client->dev, "get cw_capacity error; cw_capacity = %d\n", cw_capacity);
#endif
                reset_loop++;
                
            if (reset_loop >5){ 
            	
                reset_val = MODE_SLEEP;               
                ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
                if (ret < 0)
                    return ret;
                reset_val = MODE_NORMAL;
                msleep(10);
                ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
                if (ret < 0)
                    return ret;
                                              
                ret = cw_init(cw_bat);
                   if (ret) 
                     return ret;
                reset_loop =0;               
            }
                                     
            return cw_capacity;
        }else {
        	reset_loop =0;
        }

        if (cw_capacity == 0) 
#ifdef FG_CW2015_DEBUG
                FG_CW2015_LOG("the cw201x capacity is 0 !!!!!!!, funciton: %s, line: %d\n", __func__, __LINE__);
#else
                //dev_dbg(&cw_bat->client->dev, "the cw201x capacity is 0 !!!!!!!, funciton: %s, line: %d\n", __func__, __LINE__);
#endif
        else 
#ifdef FG_CW2015_DEBUG
                FG_CW2015_LOG("the cw201x capacity is %d, funciton: %s\n", cw_capacity, __func__);
#else
                //dev_dbg(&cw_bat->client->dev, "the cw201x capacity is %d, funciton: %s\n", cw_capacity, __func__);
#endif

        // ret = cw_read(cw_bat->client, REG_SOC + 1, &reg_val);
#ifdef  BAT_CHANGE_ALGORITHM
        
        if( g_platform_boot_mode == 8 )
        {PowerResetFlag = -1; 
		count_real_sum = 26;
		}else{
				count_real_sum = 5;
		}
        cw_capacity = cw_algorithm(cw_bat,cw_capacity);
            
#endif

        ktime_get_ts(&ts);
        new_run_time = ts.tv_sec;

        get_monotonic_boottime(&ts);
        new_sleep_time = ts.tv_sec - new_run_time;
        FG_CW2015_LOG("cw_get_capacity cw_bat->charger_mode = %d\n",cw_bat->charger_mode);
        //count_time == 20s  

#if 0
        count_time_sum=count_time*(queue_delayed_work_time/125);
         if(count_time_sum<42)
             count_time++;
        FG_CW2015_LOG("cw2015(count_time_sum<35s) count_time_sum = %d\n",count_time_sum);
#endif
      if(count_real_capacity <= count_real_sum) {
             count_real_capacity++;
        FG_CW2015_LOG("count_real_capacity = %d\n",cw_bat->charger_mode);
	  }

#ifdef CONFIG_CM865_MAINBOARD //add by longcheer_liml_2015_10_12
	if(Charger_enable_Flag==0)
	{
		if (
			(cw_bat->charger_mode == 0) && (cw_capacity > cw_bat->capacity)&&(cw_capacity < (cw_bat->capacity+20))&&(count_real_capacity>= count_real_sum ) )
		{             // modify battery level swing
			if (!(cw_capacity == 0 && cw_bat->capacity <= 2)) 
			{			
				cw_capacity = cw_bat->capacity;
			}
		} 
	}else
	{
		if (
#ifdef CHARGING_NO_DOWN_CAP //liuchao
		((cw_bat->charger_mode > 0) && (cw_capacity <= (cw_bat->capacity - 1)) && (cw_capacity > (cw_bat->capacity - 9)))
                        || 
#endif
((cw_bat->charger_mode == 0) && (cw_capacity > cw_bat->capacity)&&(cw_capacity < (cw_bat->capacity+20))&&(count_real_capacity>= count_real_sum ) )) 
		{             // modify battery level swing
			if (!(cw_capacity == 0 && cw_bat->capacity <= 2)) 
			{			
				cw_capacity = cw_bat->capacity;
			}	
		} 
	}

#else
	if (
#ifdef CHARGING_NO_DOWN_CAP //liuchao
		((cw_bat->charger_mode > 0) && (cw_capacity <= (cw_bat->capacity - 1)) && (cw_capacity > (cw_bat->capacity - 9)))
                        || 
#endif
//((cw_bat->charger_mode == 0) && (cw_capacity == (cw_bat->capacity + 1)))) {             // modify battery level swing
((cw_bat->charger_mode == 0) && (cw_capacity > cw_bat->capacity)&&(cw_capacity < (cw_bat->capacity+20))&&(count_real_capacity>= count_real_sum ) )) {             // modify battery level swing
		if (!(cw_capacity == 0 && cw_bat->capacity <= 2)) 
		{			
			cw_capacity = cw_bat->capacity;
		}
	} 
#endif


        if ((cw_bat->charger_mode > 0) && (cw_capacity >= 95) && (cw_capacity <= cw_bat->capacity)) {     // avoid no charge full

                capacity_or_aconline_time = (cw_bat->sleep_time_capacity_change > cw_bat->sleep_time_charge_start) ? cw_bat->sleep_time_capacity_change : cw_bat->sleep_time_charge_start;
                capacity_or_aconline_time += (cw_bat->run_time_capacity_change > cw_bat->run_time_charge_start) ? cw_bat->run_time_capacity_change : cw_bat->run_time_charge_start;
                allow_change = (new_sleep_time + new_run_time - capacity_or_aconline_time) / BATTERY_UP_MAX_CHANGE;
                if (allow_change > 0) {
                        allow_capacity = cw_bat->capacity + allow_change; 
                        cw_capacity = (allow_capacity <= 100) ? allow_capacity : 100;
                        jump_flag =1;
                } else if (cw_capacity <= cw_bat->capacity) {
                        cw_capacity = cw_bat->capacity; 
                }

        }
       
        else if ((cw_bat->charger_mode == 0) && (cw_capacity <= cw_bat->capacity ) && (cw_capacity >= 90) && (jump_flag == 1)) {     // avoid battery level jump to CW_BAT
                capacity_or_aconline_time = (cw_bat->sleep_time_capacity_change > cw_bat->sleep_time_charge_start) ? cw_bat->sleep_time_capacity_change : cw_bat->sleep_time_charge_start;
                capacity_or_aconline_time += (cw_bat->run_time_capacity_change > cw_bat->run_time_charge_start) ? cw_bat->run_time_capacity_change : cw_bat->run_time_charge_start;
                allow_change = (new_sleep_time + new_run_time - capacity_or_aconline_time) / BATTERY_DOWN_CHANGE;
                if (allow_change > 0) {
                        allow_capacity = cw_bat->capacity - allow_change; 
                        if (cw_capacity >= allow_capacity){
                        	jump_flag =0;
                        }
                        else{
                                cw_capacity = (allow_capacity <= 100) ? allow_capacity : 100;
                        }
                } else if (cw_capacity <= cw_bat->capacity) {
                        cw_capacity = cw_bat->capacity;
                }
        }
				
				if ((cw_capacity == 0) && (cw_bat->capacity > 1)) {              // avoid battery level jump to 0% at a moment from more than 2%
                allow_change = ((new_run_time - cw_bat->run_time_capacity_change) / BATTERY_DOWN_MIN_CHANGE_RUN);
                allow_change += ((new_sleep_time - cw_bat->sleep_time_capacity_change) / BATTERY_DOWN_MIN_CHANGE_SLEEP);

                allow_capacity = cw_bat->capacity - allow_change;
                cw_capacity = (allow_capacity >= cw_capacity) ? allow_capacity: cw_capacity;
#ifdef FG_CW2015_DEBUG
                FG_CW2015_LOG("report GGIC POR happened\n");
#else
                //FG_CW2015_ERR("report GGIC POR happened");
#endif

                reset_val = MODE_SLEEP;               
                ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
                if (ret < 0)
                    return ret;
                reset_val = MODE_NORMAL;
                msleep(10);
                ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
                if (ret < 0)
                    return ret;
                                              
                ret = cw_init(cw_bat);
                   if (ret) 
                    return ret;
                                               
        }
 
#if 1	
	if((cw_bat->charger_mode > 0) &&(cw_capacity == 0))
	{		  
                charge_time = new_sleep_time + new_run_time - cw_bat->sleep_time_charge_start - cw_bat->run_time_charge_start;
                if ((charge_time > BATTERY_DOWN_MAX_CHANGE_RUN_AC_ONLINE) && (if_quickstart == 0)) {
        		      reset_val = MODE_SLEEP;               
                ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
                if (ret < 0)
                    return ret;
                reset_val = MODE_NORMAL;
                msleep(10);
                ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
                if (ret < 0)
                    return ret;
                                              
                ret = cw_init(cw_bat);
                   if (ret) 
                    return ret;      // if the cw_capacity = 0 the cw2015 will qstrt
#ifdef FG_CW2015_DEBUG
        		      FG_CW2015_LOG("report battery capacity still 0 if in changing\n");
#else
        		      //FG_CW2015_ERR("report battery capacity still 0 if in changing");
#endif
                        if_quickstart = 1;
                }
	} else if ((if_quickstart == 1)&&(cw_bat->charger_mode == 0)) {
    		if_quickstart = 0;
        }

#endif

#ifdef SYSTEM_SHUTDOWN_VOLTAGE
        if ((cw_bat->charger_mode == 0) && (cw_capacity <= 20) && (cw_bat->voltage <= SYSTEM_SHUTDOWN_VOLTAGE)){      	     
                if (if_quickstart == 10){  
                	
                    allow_change = ((new_run_time - cw_bat->run_time_capacity_change) / BATTERY_DOWN_MIN_CHANGE_RUN);
                    allow_change += ((new_sleep_time - cw_bat->sleep_time_capacity_change) / BATTERY_DOWN_MIN_CHANGE_SLEEP);

                    allow_capacity = cw_bat->capacity - allow_change;
                    cw_capacity = (allow_capacity >= 0) ? allow_capacity: 0;
                	
                    if (cw_capacity < 1){	     	      	
                        cw_quickstart(cw_bat);
                        if_quickstart = 12;
                        cw_capacity = 0;
                    }
                } else if (if_quickstart <= 10)
                        if_quickstart =if_quickstart+2;
#ifdef FG_CW2015_DEBUG
                FG_CW2015_LOG("the cw201x voltage is less than SYSTEM_SHUTDOWN_VOLTAGE !!!!!!!, funciton: %s, line: %d\n", __func__, __LINE__);
#else
                //FG_CW2015_ERR("the cw201x voltage is less than SYSTEM_SHUTDOWN_VOLTAGE !!!!!!!, funciton: %s, line: %d\n", __func__, __LINE__);
#endif
        } else if ((cw_bat->charger_mode > 0)&& (if_quickstart <= 12)) {
                if_quickstart = 0;
        }
#endif
        return cw_capacity;
}
int cw2015_check=0;
static int cw_get_vol(struct cw_battery *cw_bat)
{
        int ret;
        u8 reg_val[2];
        u16 value16, value16_1, value16_2, value16_3;
        int voltage;
	FG_CW2015_LOG("cw_get_vol \n");
        ret = cw_read_word(cw_bat->client, REG_VCELL, reg_val);
        if (ret < 0)
        	{
        		FG_CW2015_LOG("cw_get_vol 1111\n");
                return ret;
        	}
        value16 = (reg_val[0] << 8) + reg_val[1];
        
        ret = cw_read_word(cw_bat->client, REG_VCELL, reg_val);
        if (ret < 0)
        	{
        	  FG_CW2015_LOG("cw_get_vol 2222\n");
                return ret;
        	}
        value16_1 = (reg_val[0] << 8) + reg_val[1];

        ret = cw_read_word(cw_bat->client, REG_VCELL, reg_val);
        if (ret < 0)
        	{
        	 FG_CW2015_LOG("cw_get_vol 3333\n");
                return ret;
        	}
        value16_2 = (reg_val[0] << 8) + reg_val[1];
		
		
        if(value16 > value16_1)
	    {	 
	    	value16_3 = value16;
		    value16 = value16_1;
		    value16_1 = value16_3;
        }
		
        if(value16_1 > value16_2)
	    {
	    	value16_3 =value16_1;
			value16_1 =value16_2;
			value16_2 =value16_3;
	    }
			
        if(value16 >value16_1)
	    {	 
	    	value16_3 =value16;
			value16 =value16_1;
			value16_1 =value16_3;
        }			

        voltage = value16_1 * 312 / 1024;
        voltage = voltage;// * 1000;
	FG_CW2015_LOG("cw_get_vol 4444 voltage = %d\n",voltage);
	if(voltage ==0)
	cw2015_check++;
        return voltage;
}

#ifdef BAT_LOW_INTERRUPT
static int cw_get_alt(struct cw_battery *cw_bat)
{
        int ret = 0;
        u8 reg_val;
        u8 value8 = 0;
        int alrt;
        
        ret = cw_read(cw_bat->client, REG_RRT_ALERT, &reg_val);
        if (ret < 0)
                return ret;
        value8 = reg_val;
        alrt = value8 >>7;
        
        //FG_CW2015_ERR("read RRT %d%%. value16 0x%x\n", alrt, value16);
        value8 = value8&0x7f;
        reg_val = value8;
        ret = cw_write(cw_bat->client, REG_RRT_ALERT, &reg_val);
        if(ret < 0) {
#ifdef FG_CW2015_DEBUG
                FG_CW2015_ERR( "Error clear ALRT\n");
#else
                //dev_err(&cw_bat->client->dev, "Error clear ALRT\n");
#endif
                return ret;
        }
        
        return alrt;
}
#endif


static int cw_get_time_to_empty(struct cw_battery *cw_bat)
{
        int ret;
        u8 reg_val;
        u16 value16;

        ret = cw_read(cw_bat->client, REG_RRT_ALERT, &reg_val);
        if (ret < 0)
                return ret;

        value16 = reg_val;

        ret = cw_read(cw_bat->client, REG_RRT_ALERT + 1, &reg_val);
        if (ret < 0)
                return ret;

        value16 = ((value16 << 8) + reg_val) & 0x1fff;
        return value16;
}

static void rk_bat_update_capacity(struct cw_battery *cw_bat)
{
        int cw_capacity;
#ifdef  BAT_CHANGE_ALGORITHM
        //cw_capacity = cw_algorithm(cw_bat,cw_get_capacity(cw_bat));
        cw_capacity = cw_get_capacity(cw_bat);
        FG_CW2015_ERR("cw2015_file_test userdata,	%ld,	%d,	%d\n",get_seconds(),cw_capacity,cw_bat->voltage);
        
#else
        cw_capacity = cw_get_capacity(cw_bat);
#endif
        if ((cw_capacity >= 0) && (cw_capacity <= 100) && (cw_bat->capacity != cw_capacity)) {
                cw_bat->capacity = cw_capacity;
                cw_bat->bat_change = 1;
                cw_update_time_member_capacity_change(cw_bat);

                if (cw_bat->capacity == 0)
#ifdef FG_CW2015_DEBUG
                        FG_CW2015_LOG("report battery capacity 0 and will shutdown if no changing\n");
#else
                        //FG_CW2015_ERR("report battery capacity 0 and will shutdown if no changing");
#endif

        }
	FG_CW2015_LOG("rk_bat_update_capacity cw_capacity = %d\n",cw_bat->capacity);
}



static void rk_bat_update_vol(struct cw_battery *cw_bat)
{
        int ret;
        ret = cw_get_vol(cw_bat);
        if ((ret >= 0) && (cw_bat->voltage != ret)) {
                cw_bat->voltage = ret;
                cw_bat->bat_change = 1;
        }
}

static void rk_bat_update_status(struct cw_battery *cw_bat)
{
        int status;


        if (cw_bat->charger_mode > 0) {
                if (cw_bat->capacity >= 100) 
                        status=POWER_SUPPLY_STATUS_FULL;
                else
                        status=POWER_SUPPLY_STATUS_CHARGING;
        } else {
                status = POWER_SUPPLY_STATUS_NOT_CHARGING;
        }

        if (cw_bat->status != status) {
                cw_bat->status = status;
                cw_bat->bat_change = 1;
       
        } 
}

static void rk_bat_update_time_to_empty(struct cw_battery *cw_bat)
{
        int ret;
        ret = cw_get_time_to_empty(cw_bat);
        if ((ret >= 0) && (cw_bat->time_to_empty != ret)) {
                cw_bat->time_to_empty = ret;
                cw_bat->bat_change = 1;
        }
        
}

static int rk_ac_update_online(struct cw_battery *cw_bat)
{
        int ret = 0;
#if 0
        if(cw_bat->plat_data->dc_det_pin == INVALID_GPIO) {
                cw_bat->dc_online = 0;
                return 0;
        }

        if (cw_bat->plat_data->is_dc_charge == 0) {
                cw_bat->dc_online = 0;
                return 0;
        }


        if (gpio_get_value(cw_bat->plat_data->dc_det_pin) == cw_bat->plat_data->dc_det_level) {
                if (cw_bat->dc_online != 1) {
                        cw_update_time_member_charge_start(cw_bat);
                        cw_bat->dc_online = 1;
                        if (cw_bat->charger_mode != AC_CHARGER_MODE)
                                cw_bat->charger_mode = AC_CHARGER_MODE;
 
                        ret = 1;
                }
        } else {
                if (cw_bat->dc_online != 0) {
                        cw_update_time_member_charge_start(cw_bat);
                        cw_bat->dc_online = 0;
                        if (cw_bat->usb_online == 0)
                                cw_bat->charger_mode = 0;
                        ret = 1;
                }
        }
#endif
        return ret;
}

static int get_usb_charge_state(struct cw_battery *cw_bat)
{
#if 0
#if 0  
        int charge_time;
        int time_from_boot;
        struct timespec ts;

        int gadget_status = get_gadget_connect_flag();
        int usb_status = dwc_vbus_status();

        get_monotonic_boottime(&ts);
        time_from_boot = ts.tv_sec;
      
        if (cw_bat->charger_init_mode) {
 
                if (usb_status == 1 || usb_status == 2) {
                        cw_bat->charger_init_mode = 0;
                } else if (time_from_boot < 8) {
                        usb_status = cw_bat->charger_init_mode;
                } else if (strstr(saved_command_line,"charger")) {
                        cw_bat->charger_init_mode = dwc_otg_check_dpdm();
                        usb_status = cw_bat->charger_init_mode;
                }
        }
#ifdef NO_STANDARD_AC_BIG_CHARGE_MODE 
        if (cw_bat->usb_online == 1) {
                
                charge_time = time_from_boot - cw_bat->sleep_time_charge_start - cw_bat->run_time_charge_start;
                if (charge_time > 3) {
                        if (gadget_status == 0 && dwc_vbus_status() == 1) {
                                usb_status = 2;
                        }
                }
        }
#endif
#ifdef FG_CW2015_DEBUG
        FG_CW2015_LOG(&cw_bat->client->dev, "%s usb_status=[%d],cw_bat->charger_mode=[%d],cw_bat->gadget_status=[%d], cw_bat->charger_init_mode = [%d]\n",__func__,usb_status,cw_bat->charger_mode,gadget_status, cw_bat->charger_init_mode);
#endif
#endif
	int usb_status  = 2;
        return usb_status;
 
        //dev_dbg(&cw_bat->client->dev, "%s usb_status=[%d],cw_bat->charger_mode=[%d],cw_bat->gadget_status=[%d], cw_bat->charger_init_mode = [%d]\n",__func__,usb_status,cw_bat->charger_mode,gadget_status, cw_bat->charger_init_mode);
#else
	 int usb_status = 0;//dwc_vbus_status();
	 FG_CW2015_LOG("get_usb_charge_state FG_charging_type = %d\n",FG_charging_type);
	 if(FG_charging_status == 0)
	 {
	     	usb_status = 0;
		cw_bat->charger_mode = 0;
	 }
	 else
	 {
	 	if(FG_charging_type==STANDARD_HOST)
 	 	{
	 		usb_status = 1;
			cw_bat->charger_mode = USB_CHARGER_MODE;
 	 	}
	 	//else if(FG_charging_type == STANDARD_CHARGER)
		else
	 	{
	 		usb_status = 2;
			cw_bat->charger_mode = AC_CHARGER_MODE;
	 	}
	}
 	FG_CW2015_LOG("get_usb_charge_state usb_status = %d,FG_charging_status = %d\n",usb_status,FG_charging_status);
	return usb_status;

#endif
}

static int rk_usb_update_online(struct cw_battery *cw_bat)
{
        int ret = 0;
        int usb_status = 0;

	FG_CW2015_LOG("rk_usb_update_online FG_charging_status = %d\n",FG_charging_status);
	
	#if 0	 
        if (cw_bat->plat_data->is_usb_charge == 0) {
                cw_bat->usb_online = 0;
                return 0;

        }
        #endif
		
        usb_status = get_usb_charge_state(cw_bat);        
        if (usb_status == 2) {
                if (cw_bat->charger_mode != AC_CHARGER_MODE) {
                        cw_bat->charger_mode = AC_CHARGER_MODE;
                        ret = 1;
                }
//                if (cw_bat->plat_data->chg_mode_sel_pin != INVALID_GPIO) {
//                        if (gpio_get_value (cw_bat->plat_data->chg_mode_sel_pin) != cw_bat->plat_data->chg_mode_sel_level)
//                                gpio_direction_output(cw_bat->plat_data->chg_mode_sel_pin, (cw_bat->plat_data->chg_mode_sel_level==GPIO_HIGH) ? GPIO_HIGH : GPIO_LOW);
//                }
                
                if (cw_bat->usb_online != 1) {
                        cw_bat->usb_online = 1;
                        cw_update_time_member_charge_start(cw_bat);
                }
                
        } else if (usb_status == 1) {
                if (cw_bat->charger_mode != USB_CHARGER_MODE) {
                        cw_bat->charger_mode = USB_CHARGER_MODE;
                        ret = 1;
                }
                
//                if (cw_bat->plat_data->chg_mode_sel_pin != INVALID_GPIO) {
//                        if (gpio_get_value (cw_bat->plat_data->chg_mode_sel_pin) == cw_bat->plat_data->chg_mode_sel_level)
//                                gpio_direction_output(cw_bat->plat_data->chg_mode_sel_pin, (cw_bat->plat_data->chg_mode_sel_level==GPIO_HIGH) ? GPIO_LOW : GPIO_HIGH);
//                }
                if (cw_bat->usb_online != 1){
                        cw_bat->usb_online = 1;
                        cw_update_time_member_charge_start(cw_bat);
                }

        } else if (usb_status == 0 && cw_bat->usb_online != 0) {

//                if (cw_bat->plat_data->chg_mode_sel_pin != INVALID_GPIO) {
//                        if (gpio_get_value (cw_bat->plat_data->chg_mode_sel_pin == cw_bat->plat_data->chg_mode_sel_level))
//                                gpio_direction_output(cw_bat->plat_data->chg_mode_sel_pin, (cw_bat->plat_data->chg_mode_sel_level==GPIO_HIGH) ? GPIO_LOW : GPIO_HIGH);
//                }

//                if (cw_bat->usb_online == 0)
                        cw_bat->charger_mode = 0;

                cw_update_time_member_charge_start(cw_bat);
                cw_bat->usb_online = 0;
                ret = 1;
        }

        return ret;
}

static void cw_bat_work(struct work_struct *work)
{
        struct delayed_work *delay_work;
        struct cw_battery *cw_bat;
        int ret;
        static int count_real_capacity = 0;
	FG_CW2015_FUN(); 
	printk("cw_bat_work\n");

        delay_work = container_of(work, struct delayed_work, work);
        cw_bat = container_of(delay_work, struct cw_battery, battery_delay_work);
	//printk("cw_bat_work 111\n");
        ret = rk_usb_update_online(cw_bat);
        if (ret == 1) {
	//	printk("cw_bat_work 222\n");
                //power_supply_changed(&cw_bat->rk_ac);
        }
	//printk("cw_bat_work 333\n");

	//FG_CW2015_LOG("cw_bat_work FG_charging_status = %d\n",FG_charging_status);
	//cw_bat->usb_online = FG_charging_status;
        if (cw_bat->usb_online == 1) {
	//	printk("cw_bat_work 444\n");
                ret = rk_usb_update_online(cw_bat);
                if (ret == 1) {
			//	printk("cw_bat_work 555\n");
                        //power_supply_changed(&cw_bat->rk_usb);     
                        //power_supply_changed(&cw_bat->rk_ac);
                }
        }

	//printk("cw_bat_work 666\n");
        //rk_bat_update_status(cw_bat);
        rk_bat_update_capacity(cw_bat);
        rk_bat_update_vol(cw_bat);
	g_cw2015_capacity = cw_bat->capacity;
	g_cw2015_vol = cw_bat->voltage;
       // rk_bat_update_time_to_empty(cw_bat);
	printk("cw_bat_work 777 vol = %d,cap = %d\n",cw_bat->voltage,cw_bat->capacity);
        if (cw_bat->bat_change) {
	//	printk("cw_bat_work 888\n");
                //power_supply_changed(&cw_bat->rk_bat); 
                cw_bat->bat_change = 0;
        }
        //warning: count_time 
    if(count_real_capacity < 30 && g_platform_boot_mode == 8){
        queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(1000));
	    count_real_capacity++;
    }else{
        queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(queue_delayed_work_time));
    }
/*
        queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(1000));
#ifdef FG_CW2015_DEBUG
        FG_CW2015_LOG("cw_bat->bat_change = %d, cw_bat->time_to_empty = %d, cw_bat->capacity = %d, cw_bat->voltage = %d, cw_bat->dc_online = %d, cw_bat->usb_online = %d\n",\
                        cw_bat->bat_change, cw_bat->time_to_empty, cw_bat->capacity, cw_bat->voltage, cw_bat->dc_online, cw_bat->usb_online);
#else
        //dev_dbg(&cw_bat->client->dev, "cw_bat->bat_change = %d, cw_bat->time_to_empty = %d, cw_bat->capacity = %d, cw_bat->voltage = %d, cw_bat->dc_online = %d, cw_bat->usb_online = %d\n",\
       //                 cw_bat->bat_change, cw_bat->time_to_empty, cw_bat->capacity, cw_bat->voltage, cw_bat->dc_online, cw_bat->usb_online);
        
#endif
*/
}
/*
static int rk_usb_get_property (struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
        int ret = 0;
        struct cw_battery *cw_bat;

        cw_bat = container_of(psy, struct cw_battery, rk_usb);
        switch (psp) {
        case POWER_SUPPLY_PROP_ONLINE:
                // val->intval = cw_bat->usb_online;
                val->intval = (cw_bat->charger_mode == USB_CHARGER_MODE);   
                break;
        default:
                break;
        }
        return ret;
}

static enum power_supply_property rk_usb_properties[] = {
        POWER_SUPPLY_PROP_ONLINE,
};


static int rk_ac_get_property (struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
        int ret = 0;
        struct cw_battery *cw_bat;

        cw_bat = container_of(psy, struct cw_battery, rk_ac);
        switch (psp) {
        case POWER_SUPPLY_PROP_ONLINE:
                // val->intval = cw_bat->dc_online;
                val->intval = (cw_bat->charger_mode == AC_CHARGER_MODE);
                break;
        default:
                break;
        }
        return ret;
}

static enum power_supply_property rk_ac_properties[] = {
        POWER_SUPPLY_PROP_ONLINE,
};
*/
static int rk_battery_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
        int ret = 0;
        /*
        struct cw_battery *cw_bat;

        cw_bat = container_of(psy, struct cw_battery, rk_bat); 
        switch (psp) {
        case POWER_SUPPLY_PROP_CAPACITY:
                val->intval = cw_bat->capacity;
                break;
        case POWER_SUPPLY_PROP_STATUS:
                val->intval = cw_bat->status;
                break;
                
        case POWER_SUPPLY_PROP_HEALTH:
                val->intval= POWER_SUPPLY_HEALTH_GOOD;
                break;
        case POWER_SUPPLY_PROP_PRESENT:
                val->intval = cw_bat->voltage <= 0 ? 0 : 1;
                break;
                
        case POWER_SUPPLY_PROP_VOLTAGE_NOW:
                val->intval = cw_bat->voltage;
                break;
                
        case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
                val->intval = cw_bat->time_to_empty;			
                break;
            
        case POWER_SUPPLY_PROP_TECHNOLOGY:
                val->intval = POWER_SUPPLY_TECHNOLOGY_LION;	
                break;

        default:
                break;
        }
        */
        return ret;
}

static enum power_supply_property rk_battery_properties[] = {
/*
        POWER_SUPPLY_PROP_CAPACITY,
        POWER_SUPPLY_PROP_STATUS,
        POWER_SUPPLY_PROP_HEALTH,
        POWER_SUPPLY_PROP_PRESENT,
        POWER_SUPPLY_PROP_VOLTAGE_NOW,
        POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
        POWER_SUPPLY_PROP_TECHNOLOGY,
*/        
};

/*
static int cw_bat_gpio_init(struct cw_battery *cw_bat)
{

        int ret;
        gpio_free(cw_bat->plat_data->dc_det_pin);
        if (cw_bat->plat_data->dc_det_pin != INVALID_GPIO) {
                ret = gpio_request(cw_bat->plat_data->dc_det_pin, NULL);
                if (ret) {
#ifdef FG_CW2015_DEBUG
                        FG_CW2015_ERR("failed to request dc_det_pin gpio\n");
#else
                        //dev_err(&cw_bat->client->dev, "failed to request dc_det_pin gpio\n");
#endif
                        goto request_dc_det_pin_fail;
                }

                gpio_pull_updown(cw_bat->plat_data->dc_det_pin, GPIOPullUp);
                ret = gpio_direction_input(cw_bat->plat_data->dc_det_pin);
                if (ret) {
#ifdef FG_CW2015_DEBUG
                        FG_CW2015_ERR("failed to set dc_det_pin input\n");
#else
                        //dev_err(&cw_bat->client->dev, "failed to set dc_det_pin input\n");
#endif
                        goto request_bat_low_pin_fail;
                }
        }
        if (cw_bat->plat_data->bat_low_pin != INVALID_GPIO) {
                ret = gpio_request(cw_bat->plat_data->bat_low_pin, NULL);
                if (ret) {
#ifdef FG_CW2015_DEBUG
                        FG_CW2015_ERR("failed to request bat_low_pin gpio\n");
#else
                        //dev_err(&cw_bat->client->dev, "failed to request bat_low_pin gpio\n");
#endif
                        goto request_bat_low_pin_fail;
                }

                gpio_pull_updown(cw_bat->plat_data->bat_low_pin, GPIOPullUp);
                ret = gpio_direction_input(cw_bat->plat_data->bat_low_pin);
                if (ret) {
#ifdef FG_CW2015_DEBUG
                        FG_CW2015_ERR("failed to set bat_low_pin input\n");
#else
                        //dev_err(&cw_bat->client->dev, "failed to set bat_low_pin input\n");
#endif
                        goto request_chg_ok_pin_fail;
                }
        }
        if (cw_bat->plat_data->chg_ok_pin != INVALID_GPIO) {
                ret = gpio_request(cw_bat->plat_data->chg_ok_pin, NULL);
                if (ret) {
#ifdef FG_CW2015_DEBUG
                        FG_CW2015_ERR("failed to request chg_ok_pin gpio\n");
#else
                        //dev_err(&cw_bat->client->dev, "failed to request chg_ok_pin gpio\n");
#endif
                        goto request_chg_ok_pin_fail;
                }

                gpio_pull_updown(cw_bat->plat_data->chg_ok_pin, GPIOPullUp);
                ret = gpio_direction_input(cw_bat->plat_data->chg_ok_pin);
                if (ret) {
#ifdef FG_CW2015_DEBUG
                        FG_CW2015_ERR("failed to set chg_ok_pin input\n");
#else
                        //dev_err(&cw_bat->client->dev, "failed to set chg_ok_pin input\n");
#endif
                        gpio_free(cw_bat->plat_data->chg_ok_pin); 
                        goto request_chg_ok_pin_fail;
                }
        }

        if ((cw_bat->Usb_online == 1) && (cw_bat->plat_data->chg_mode_sel_pin!= INVALID_GPIO)) {
                ret = gpio_request(cw_bat->plat_data->chg_mode_sel_pin, NULL);
                if (ret) {
#ifdef FG_CW2015_DEBUG
                        FG_CW2015_ERR("failed to request chg_mode_sel_pin gpio\n");
#else
                        //dev_err(&cw_bat->client->dev, "failed to request chg_mode_sel_pin gpio\n");
#endif
                        goto request_chg_ok_pin_fail;
                }
                ret = gpio_direction_output(cw_bat->plat_data->chg_mode_sel_pin, (cw_bat->plat_data->chg_mode_sel_level==GPIO_HIGH) ? GPIO_LOW : GPIO_HIGH);
                if (ret) {
#ifdef FG_CW2015_DEBUG
                        FG_CW2015_ERR("failed to set chg_mode_sel_pin input\n");
#else
                        //dev_err(&cw_bat->client->dev, "failed to set chg_mode_sel_pin input\n");
#endif
                        gpio_free(cw_bat->plat_data->chg_mode_sel_pin); 
                        goto request_chg_ok_pin_fail;
                }
        }
 
        return 0;

        
request_chg_ok_pin_fail:
        if (cw_bat->plat_data->bat_low_pin != INVALID_GPIO)
                gpio_free(cw_bat->plat_data->bat_low_pin);

request_bat_low_pin_fail:
        if (cw_bat->plat_data->dc_det_pin != INVALID_GPIO) 
                gpio_free(cw_bat->plat_data->dc_det_pin);

request_dc_det_pin_fail:
        return ret;

}


static void dc_detect_do_wakeup(struct work_struct *work)
{
        int ret;
        int irq;
        unsigned int type;

        struct delayed_work *delay_work;
        struct cw_battery *cw_bat;

        delay_work = container_of(work, struct delayed_work, work);
        cw_bat = container_of(delay_work, struct cw_battery, dc_wakeup_work);

        rk28_send_wakeup_key();

        // this assume if usb insert or extract dc_det pin is change 
#if 0
        if(cw_bat->charger_init_mode)
                cw_bat->charger_init_mode=0;
#endif

        irq = gpio_to_irq(cw_bat->plat_data->dc_det_pin);
        type = gpio_get_value(cw_bat->plat_data->dc_det_pin) ? IRQ_TYPE_EDGE_FALLING : IRQ_TYPE_EDGE_RISING;
        ret = irq_set_irq_type(irq, type);
        if (ret < 0) {
#ifdef FG_CW2015_DEBUG
                FG_CW2015_ERR("%s: irq_set_irq_type(%d, %d) failed\n", __func__, irq, type);
#else
                //pr_err("%s: irq_set_irq_type(%d, %d) failed\n", __func__, irq, type);
#endif
        }
        enable_irq(irq);
}

static irqreturn_t dc_detect_irq_handler(int irq, void *dev_id)
{
        struct cw_battery *cw_bat = dev_id;
        disable_irq_nosync(irq); // for irq debounce
        queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->dc_wakeup_work, msecs_to_jiffies(20));
        return IRQ_HANDLED;
}

#ifdef BAT_LOW_INTERRUPT

#define WAKE_LOCK_TIMEOUT       (10 * HZ)
static struct wake_lock bat_low_wakelock;

static void bat_low_detect_do_wakeup(struct work_struct *work)
{
        struct delayed_work *delay_work;
        struct cw_battery *cw_bat;

        delay_work = container_of(work, struct delayed_work, work);
        cw_bat = container_of(delay_work, struct cw_battery, bat_low_wakeup_work);
#ifdef FG_CW2015_DEBUG
        FG_CW2015_LOG("func: %s-------\n", __func__);
#else
        //FG_CW2015_ERR("func: %s-------\n", __func__);
#endif
        cw_get_alt(cw_bat);
        //enable_irq(irq);
}

static irqreturn_t bat_low_detect_irq_handler(int irq, void *dev_id)
{
        struct cw_battery *cw_bat = dev_id;
        // disable_irq_nosync(irq); // for irq debounce
        wake_lock_timeout(&bat_low_wakelock, WAKE_LOCK_TIMEOUT);
        queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->bat_low_wakeup_work, msecs_to_jiffies(20));
        return IRQ_HANDLED;
}
#endif
*/
/*----------------------------------------------------------------------------*/
static int cw2015_i2c_detect(struct i2c_client *client, struct i2c_board_info *info) 
{    
	FG_CW2015_FUN(); 
	//printk("cw2015_i2c_detect\n");
	
	strcpy(info->type, CW2015_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/

/*Chaman add for create sysfs start*/

static ssize_t file_state_show(struct device *d, struct device_attribute *a, char *buf)
{

return		sprintf(buf, "%d", file_sys_state);
}
static DEVICE_ATTR(file_state, S_IRUGO, file_state_show, NULL);
/*Chaman add for create sysfs end*/


static int cw2015_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        struct cw_battery *cw_bat;
        int ret;
        int irq;
        int irq_flags;
        int loop = 0;
	FG_CW2015_FUN(); 
	//printk("cw2015_i2c_probe\n");
    mt_set_gpio_mode(GPIO1, 3);
    mt_set_gpio_mode(GPIO2, 3);

		file_sys_state = 1;

        //cw_bat = devm_kzalloc(&client->dev, sizeof(*cw_bat), GFP_KERNEL);
		 cw_bat = kzalloc(sizeof(struct cw_battery), GFP_KERNEL);
        if (!cw_bat) {
#ifdef FG_CW2015_DEBUG
                FG_CW2015_ERR("fail to allocate memory\n");
#else
                //dev_err(&cw_bat->client->dev, "fail to allocate memory\n");
#endif
                return -ENOMEM;
        }
				
		 //memset(data, 0, sizeof(*cw_bat));
		 
        i2c_set_clientdata(client, cw_bat);
        cw_bat->plat_data = client->dev.platform_data;
/*
        ret = cw_bat_gpio_init(cw_bat);
        if (ret) {
#ifdef FG_CW2015_DEBUG
                FG_CW2015_ERR("cw_bat_gpio_init error\n");
#else
                //dev_err(&cw_bat->client->dev, "cw_bat_gpio_init error\n");
#endif
                return ret;
        }
*/
        cw_bat->client = client;
	cw_bat->plat_data = &cw_bat_platdata;
        ret = cw_init(cw_bat);
       // while ((loop++ < 2000) && (ret != 0)) {
        //        ret = cw_init(cw_bat);
       // }

        if (ret) 
                return ret;
/*        
        cw_bat->rk_bat.name = "rk-bat";
        cw_bat->rk_bat.type = POWER_SUPPLY_TYPE_BATTERY;
        cw_bat->rk_bat.properties = rk_battery_properties;
        cw_bat->rk_bat.num_properties = ARRAY_SIZE(rk_battery_properties);
        cw_bat->rk_bat.get_property = rk_battery_get_property;
        ret = power_supply_register(&client->dev, &cw_bat->rk_bat);
        if(ret < 0) {
#ifdef FG_CW2015_DEBUG
                FG_CW2015_ERR("power supply register rk_bat error\n");
#else
                //dev_err(&cw_bat->client->dev, "power supply register rk_bat error\n");
#endif
                goto rk_bat_register_fail;
        }

        cw_bat->rk_ac.name = "rk-ac";
        cw_bat->rk_ac.type = POWER_SUPPLY_TYPE_MAINS;
        cw_bat->rk_ac.properties = rk_ac_properties;
        cw_bat->rk_ac.num_properties = ARRAY_SIZE(rk_ac_properties);
        cw_bat->rk_ac.get_property = rk_ac_get_property;
        ret = power_supply_register(&client->dev, &cw_bat->rk_ac);
        if(ret < 0) {
#ifdef FG_CW2015_DEBUG
                FG_CW2015_ERR("power supply register rk_ac error\n");
#else
                //dev_err(&cw_bat->client->dev, "power supply register rk_ac error\n");
#endif
                goto rk_ac_register_fail;
        }

        cw_bat->rk_usb.name = "rk-usb";
        cw_bat->rk_usb.type = POWER_SUPPLY_TYPE_USB;
        cw_bat->rk_usb.properties = rk_usb_properties;
        cw_bat->rk_usb.num_properties = ARRAY_SIZE(rk_usb_properties);
        cw_bat->rk_usb.get_property = rk_usb_get_property;
        ret = power_supply_register(&client->dev, &cw_bat->rk_usb);
        if(ret < 0) {
#ifdef FG_CW2015_DEBUG
                FG_CW2015_ERR("power supply register rk_ac error\n");
#else
                //dev_err(&cw_bat->client->dev, "power supply register rk_ac error\n");
#endif
                goto rk_usb_register_fail;
        }

        cw_bat->charger_init_mode = dwc_otg_check_dpdm();
*/
        cw_bat->dc_online = 0;
        cw_bat->usb_online = 0;
        cw_bat->charger_mode = 0;
        cw_bat->capacity = 1;
        cw_bat->voltage = 0;
        cw_bat->status = 0;
        cw_bat->time_to_empty = 0;
        cw_bat->bat_change = 0;

        cw_update_time_member_capacity_change(cw_bat);
        cw_update_time_member_charge_start(cw_bat);

	device_create_file(&client->dev, &dev_attr_file_state);

	cw_bat->battery_workqueue = create_singlethread_workqueue("rk_battery");
	INIT_DELAYED_WORK(&cw_bat->battery_delay_work, cw_bat_work);
	queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(10));
/*
        cw_bat->battery_workqueue = create_singlethread_workqueue("rk_battery");
        INIT_DELAYED_WORK(&cw_bat->battery_delay_work, cw_bat_work);
        INIT_DELAYED_WORK(&cw_bat->dc_wakeup_work, dc_detect_do_wakeup);
        queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(10));

#if 0
        if (cw_bat->plat_data->dc_det_pin != INVALID_GPIO) {
                irq = gpio_to_irq(cw_bat->plat_data->dc_det_pin);
                irq_flags = gpio_get_value(cw_bat->plat_data->dc_det_pin) ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;
                ret = request_irq(irq, dc_detect_irq_handler, irq_flags, "usb_detect", cw_bat);
                if (ret < 0) {
#ifdef FG_CW2015_DEBUG
                        FG_CW2015_ERR("request_irq failed\n",);
#else
                        //pr_err("%s: request_irq(%d) failed\n", __func__, irq);
#endif
                }
                enable_irq_wake(irq);
        }
#else
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, TRUE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);

	mt_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_TYPE, apds9960_interrupt, 0);

	mt_eint_unmask(CUST_EINT_ALS_NUM);  

#endif
#ifdef BAT_LOW_INTERRUPT
        INIT_DELAYED_WORK(&cw_bat->bat_low_wakeup_work, bat_low_detect_do_wakeup);
        wake_lock_init(&bat_low_wakelock, WAKE_LOCK_SUSPEND, "bat_low_detect");
        if (cw_bat->plat_data->bat_low_pin != INVALID_GPIO) {
                irq = gpio_to_irq(cw_bat->plat_data->bat_low_pin);
                ret = request_irq(irq, bat_low_detect_irq_handler, IRQF_TRIGGER_RISING, "bat_low_detect", cw_bat);
                if (ret < 0) {
                        gpio_free(cw_bat->plat_data->bat_low_pin);
                }
                enable_irq_wake(irq);
        }
#endif 
*/
#ifdef FG_CW2015_DEBUG
        FG_CW2015_LOG("cw2015/cw2013 driver v1.2 probe sucess\n");
#else
        //FG_CW2015_ERR("cw2015/cw2013 driver v1.2 probe sucess\n");
#endif
        return 0;

rk_usb_register_fail:
        power_supply_unregister(&cw_bat->rk_bat);
rk_ac_register_fail:
        power_supply_unregister(&cw_bat->rk_ac);
rk_bat_register_fail:
#ifdef FG_CW2015_DEBUG
        FG_CW2015_LOG("cw2015/cw2013 driver v1.2 probe error!!!!\n");
#else
        //FG_CW2015_ERR("cw2015/cw2013 driver v1.2 probe error!!!!\n");
#endif
        return ret;
}


#if 0
#ifdef CONFIG_PM
static int cw_bat_suspend(struct device *dev)
{
        struct i2c_client *client = to_i2c_client(dev);
        struct cw_battery *cw_bat = i2c_get_clientdata(client);
        dev_dbg(&cw_bat->client->dev, "%s\n", __func__);
        cancel_delayed_work(&cw_bat->battery_delay_work);
        return 0;
}

static int cw_bat_resume(struct device *dev)
{
        struct i2c_client *client = to_i2c_client(dev);
        struct cw_battery *cw_bat = i2c_get_clientdata(client);
        dev_dbg(&cw_bat->client->dev, "%s\n", __func__);
        queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(100));
        return 0;
}

#if 0
static const struct i2c_device_id cw_id[] = {
	{ "cw201x", 0 },
};
MODULE_DEVICE_TABLE(i2c, cw_id);
#endif
#endif
#endif

static int  cw2015_i2c_remove(struct i2c_client *client)//__devexit
{
	struct cw_battery *data = i2c_get_clientdata(client);

	FG_CW2015_FUN(); 
//	printk("cw2015_i2c_remove\n");
	
	//__cancel_delayed_work(&data->battery_delay_work);
	cancel_delayed_work(&data->battery_delay_work);
	cw2015_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(data);

	return 0;
}

static int cw2015_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{

  struct cw_battery *cw_bat = i2c_get_clientdata(client);
  
  FG_CW2015_FUN(); 
//	printk("cw2015_i2c_suspend\n");
  cancel_delayed_work(&cw_bat->battery_delay_work);

	return 0;
}

static int cw2015_i2c_resume(struct i2c_client *client)
{
	struct cw_battery *cw_bat = i2c_get_clientdata(client);
	
	FG_CW2015_FUN(); 
	//printk("cw2015_i2c_resume\n");
  queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(100));

	return 0;
}

static struct i2c_driver cw2015_i2c_driver = {	
	.probe      = cw2015_i2c_probe,
	.remove     = cw2015_i2c_remove,
	.detect     = cw2015_i2c_detect,
	.suspend    = cw2015_i2c_suspend,
	.resume     = cw2015_i2c_resume,
	.id_table   = FG_CW2015_i2c_id,
	.driver = {
//		.owner          = THIS_MODULE,
		.name           = CW2015_DEV_NAME,
	},
};


/*----------------------------------------------------------------------------*/
#if 0
static int cw_bat_probe(struct platform_device *pdev) 
{
    int err;
	FG_CW2015_FUN(); 
	printk("cw_bat_probe\n");
	cw2015_workqueue = create_workqueue("FG_cw2015");
	
	if (!cw2015_workqueue)
		return -ENOMEM;

	if(i2c_add_driver(&cw2015_i2c_driver))
	{
		FG_CW2015_ERR("add driver error\n");
		printk("cw_bat_probe add driver error\n");
		return -1;
	} 

       FG_CW2015_LOG("%s exit\n", __func__);
	return 0;
}

static int cw_bat_remove(struct platform_device *pdev)
{
	FG_CW2015_FUN(); 

 	if (cw2015_workqueue)
		destroy_workqueue(cw2015_workqueue);

	cw2015_workqueue = NULL;

	i2c_del_driver(&cw2015_i2c_driver);
	return 0;
}
#endif
#if 0
static struct i2c_driver cw_bat_driver = {
        .driver         = {
                .name   = "cw201x",
#ifdef CONFIG_PM
                .pm     = &cw_bat_pm_ops,
#endif
        },
        
        .probe          = cw_bat_probe,
        .remove         = cw_bat_remove,
	.id_table	= cw_id,
};
#else
#if 0
static struct platform_driver cw_bat_driver = {
	.probe      = cw_bat_probe,
	.remove     = cw_bat_remove,    
	.driver     = {
		.name  = "cw201x",
		.owner = THIS_MODULE,
	}
};
#endif
#endif
static int __init cw_bat_init(void)
{
	//return i2c_add_driver(&cw_bat_driver);
	FG_CW2015_LOG("%s: \n", __func__); 
	printk("cw_bat_init\n");
	i2c_register_board_info(4,&i2c_FG_CW2015, 1);
//	printk("cw_bat_init 111\n");
#if 0
	if(platform_driver_register(&cw_bat_driver))
	{
		FG_CW2015_ERR("failed to register driver\n");
		printk("cw_bat_init failed to register driver\n");
		return -ENODEV;
	}
#else
	if(i2c_add_driver(&cw2015_i2c_driver))
	{
		FG_CW2015_ERR("add driver error\n");
//		printk("cw_bat_init add driver error\n");
		return -1;
	}
#endif
//	printk("cw_bat_init 222\n");
	return 0;
}

static void __exit cw_bat_exit(void)
{
	FG_CW2015_LOG("%s: \n", __func__); 
	printk("cw_bat_exit\n");
        i2c_del_driver(&i2c_FG_CW2015);
       //platform_driver_unregister(&cw_bat_driver);
}
#if 0
fs_initcall(cw_bat_init);
module_exit(cw_bat_exit);

MODULE_AUTHOR("xhc<xhc@rock-chips.com>");
MODULE_DESCRIPTION("cw2015/cw2013 battery driver");
MODULE_LICENSE("GPL");
#endif

module_init(cw_bat_init);
module_exit(cw_bat_exit);

MODULE_AUTHOR("xhc<xhc@rock-chips.com>");
MODULE_DESCRIPTION("cw2015/cw2013 battery driver");
MODULE_LICENSE("GPL");

