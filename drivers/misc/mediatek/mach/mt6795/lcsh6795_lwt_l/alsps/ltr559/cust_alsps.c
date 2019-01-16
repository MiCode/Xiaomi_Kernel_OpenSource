#include <linux/types.h>
#include <mach/mt_pm_ldo.h>
#include <cust_alsps.h>
#include <mach/upmu_common.h>

static struct alsps_hw cust_alsps_hw = {
    .i2c_num    = 2,
	.polling_mode_ps =0,
	.polling_mode_als =1,
    .power_id   = MT65XX_POWER_NONE,    /*LDO is not used*/
    .power_vol  = VOL_DEFAULT,          /*LDO is not used*/
    //.i2c_addr   = {0x0C, 0x48, 0x78, 0x00},
    /* MTK: modified to support AAL */
    .als_level  = {0, 328, 861, 1377, 3125, 7721, 7767, 12621, 23062, 28430, 33274, 47116, 57694, 57694, 65535},
    .als_value  = {0, 133, 304, 502, 1004, 2005, 3058, 5005, 8008, 10010, 12000, 16000, 20000, 20000, 20000, 20000},
    .ps_threshold_high = 26,
    .ps_threshold_low = 21,
    .is_batch_supported_ps = false,
    .is_batch_supported_als = false,
};
struct alsps_hw *get_cust_alsps_hw(void) {
    return &cust_alsps_hw;
}

//add by pengc at 20150312 start
static struct alsps_hw ltr559_cust_alsps_hw = {
    .i2c_num    = 3,
    //.polling_mode = 0,
    .polling_mode_ps =0,//1,//1     
    .polling_mode_als =1,
    .power_id   = MT65XX_POWER_NONE,//MT6323_POWER_LDO_VGP1,    /*LDO is not used*/
    .power_vol  = VOL_DEFAULT,//VOL_2800,          /*LDO is not used*/
    .i2c_addr   = {0x72, 0x48, 0x78, 0x00},//0x72
    /*Lenovo-sw chenlj2 add 2011-06-03,modify parameter below two lines*/
    //.als_level  = { 0,  1,  1,   7,  15,  15,  100, 1000, 2000,  3000,  6000, 10000, 14000, 18000, 20000},
    //.als_value  = {40, 40, 90,  90, 160, 160,  225,  320,  640,  1280,  1280,  2600,  2600, 2600,  10240, 10240},
    /*Reallytek-sw huanhuan.zhuang add 2014-07-11,modify parameter below two lines*/	
#if 0
	.als_level  = {5, 10, 40, 120, 225, 550, 800, 1250, 2000, 3000, 6000, 10000, 40000, 100000, 100000},//20,  400,  620,  850,  1600,  2300, 3800, 6000,  8000,  14000, 60000, 80000, 80000, 80000},
	.als_value  = {0, 5, 10,  20,   120,  180,  280,  400,  800, 1600,  3000,  6000, 8000, 15000,  30000, 60000, 100000},

    .als_value  = {0, 5, 10, 40, 120, 225, 550, 800, 1250, 2000, 3000, 6000, 10000, 40000, 100000, 100000}, 
#endif 
	
    .als_level  =  {     1,   4,  10, 30, 120, 150, 255, 380, 650,  900, 1350, 2200, 3500, 6000, 10000, 20000, 50000},
    .als_value  = {0,  5,  10, 30, 80, 120, 225, 320, 550, 800, 1250, 2000, 3000, 5000, 5500,  7500, 13000, 24000},
//                 2   9   11  31  81  121  226  321   551  801  1251  2001  3001  5001  

//                 15  30  30  30  60   60   60   60   60   60    102   102   255   255  

    .ps_threshold_high = 1000,//700
    .ps_threshold_low = 800,//250
};
struct alsps_hw *ltr559_get_cust_alsps_hw(void) {
    return &ltr559_cust_alsps_hw;
}

//add by pengc at 20150312 end
