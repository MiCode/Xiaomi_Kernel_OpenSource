#include <linux/types.h>
#include <mach/mt_pm_ldo.h>
#include <cust_alsps.h>
#include <mach/upmu_common.h>
#if 0
static struct alsps_hw cust_alsps_hw = {
    .i2c_num    = 3,
	.polling_mode_ps =0,
	.polling_mode_als =1,
    .power_id   = MT65XX_POWER_NONE,    /*LDO is not used*/
    .power_vol  = VOL_DEFAULT,          /*LDO is not used*/
    //.i2c_addr   = {0x0C, 0x48, 0x78, 0x00},
    //.als_level  = { 0,  1,  1,   7,  15,  15,  100, 1000, 2000,  3000,  6000, 10000, 14000, 18000, 20000},
    //.als_value  = {40, 40, 90,  90, 160, 160,  225,  320,  640,  1280,  1280,  2600,  2600, 2600,  10240, 10240},
    /* MTK: modified to support AAL */
    .als_level  = {0, 194, 450, 749, 1705, 3845, 4230, 7034, 12907, 16034, 19011, 26895, 32956, 32956, 65535},
    .als_value  = {0, 133, 301, 500, 1002, 2003, 3002, 5003, 8005, 10010, 12010, 16000, 20000, 20000, 20000, 20000},
    .ps_threshold_high = 32,
    .ps_threshold_low = 22,
    .is_batch_supported_ps = false,
    .is_batch_supported_als = false,
};
struct alsps_hw *get_cust_alsps_hw(void) {
    return &cust_alsps_hw;
}
#endif
static struct alsps_hw cust_alsps_hw = {
	/* i2c bus number, for mt657x, default=0. For mt6589, default=3 */

    .i2c_num    = 3,	
	//.polling_mode =1,
	.polling_mode_ps =0,
	.polling_mode_als =1,   
    .power_id   = MT65XX_POWER_NONE,    /*LDO is not used*/
    .power_vol  = VOL_DEFAULT,          /*LDO is not used*/
    .i2c_addr   = {0x90, 0x00, 0x00, 0x00},	/*STK3x1x*/
#if 0
    .als_level  = {5,  9, 36, 59, 82, 132, 205, 273, 500, 845, 1136, 1545, 2364, 4655, 6982},	/* als_code */
    .als_value  = {10, 10, 40, 65, 90, 145, 225, 300, 550, 930, 1250, 1700, 2600, 5120, 7680, 10240},    /* lux */

    .als_level  = {1,  3, 5, 8, 11, 15, 205, 273, 500, 845, 1136, 1545, 2364, 4655, 6982},	/* als_code */
    .als_value  = {0, 3, 5, 8, 11, 15, 225, 300, 550, 930, 1250, 1700, 2600, 5120, 7680, 10240},    /* lux */

    .als_level  = {5,  10, 220, 440, 640, 850, 1280, 1660, 2500, 5000, 10000, 20000, 40000, 40000, 40000},	/* als_code */
    .als_value  = {0, 20, 250, 500, 750, 1000, 1500, 3000, 5000, 10000, 20000, 40000, 80000, 80000, 80000, 80000},    /* lux */
#endif	

#if 0
	.als_level  = {5, 10, 20,   120,  180,  280,  400,  800, 1600,  3000,  6000, 8000, 15000,  30000, 60000, 100000},//20,  400,  620,  850,  1600,  2300, 3800, 6000,  8000,  14000, 60000, 80000, 80000, 80000},
	.als_value  = {0,  5, 10,    20,   120,  180,  280,  400,  800, 1600,  3000,  6000, 8000, 15000,  30000, 60000, 100000},

    .als_level  = {7, 40, 225, 550, 930, 1250, 1500, 2000, 2500, 3000, 5000, 10000, 20000, 40000, 40000},	/* als_code */
    .als_value  = {0, 40, 225, 550, 930, 1250, 1500, 2000, 2500, 3000, 5000, 10000, 20000, 40000, 40000, 40000},    /* lux */
#endif

    .als_level  = {     1,    4, 10, 30,   80, 120, 225, 320, 550,  800, 1250, 2000, 3500, 6000, 10000, 20000, 50000},	/* als_code */
    .als_value  = {0, 5,  10, 30, 80, 120, 225, 320, 550, 800, 1250, 2000, 6000,10000,11000,15000,18000, 24000},    /* lux */
   	.state_val = 0x0,		/* disable all */
	.psctrl_val = 0x30,		/* ps_persistance=1, ps_gain=64X, PS_IT=0.391ms */
	.alsctrl_val = 0x3A,   //0x2A,	/* als_persistance=1, als_gain=16X, ALS_IT=200ms */
	.ledctrl_val = 0xFF,	/* 100mA IRDR, 64/64 LED duty */
	.wait_val = 0xF,		/* 50 ms */
    .ps_threshold_high = 1200,
    .ps_threshold_low = 1100,
};
struct alsps_hw *stk_get_cust_alsps_hw(void) {
    return &cust_alsps_hw;
}
