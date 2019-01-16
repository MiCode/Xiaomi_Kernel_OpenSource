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
#include <linux/seq_file.h>

#include <mach/system.h>
#include "mach/mtk_thermal_monitor.h"
#include "mach/mt_typedefs.h"
#include "mach/mt_thermal.h"
#include <cust_pmic.h>
//#include <mach/pmic_mt6329_hw_bank1.h>
//#include <mach/pmic_mt6329_sw_bank1.h>
//#include <mach/pmic_mt6329_hw.h>
//#include <mach/pmic_mt6329_sw.h>
#include <mach/upmu_common_sw.h>
#include <mach/upmu_hw.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/pmic_mt6331_6332_sw.h>

extern struct proc_dir_entry * mtk_thermal_get_proc_drv_therm_dir_entry(void);

static unsigned int interval = 0; /* seconds, 0 : no auto polling */
static unsigned int trip_temp[10] = {120000,110000,100000,90000,80000,70000,65000,60000,55000,50000};

static unsigned int cl_dev_sysrst_state = 0;
static struct thermal_zone_device *thz_dev;

static struct thermal_cooling_device *cl_dev_sysrst;
static int tsbuck_debug_log = 0;
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

#define TSBUCK_TEMP_CRIT 150000 /* 150.000 degree Celsius */

#define tsbuck_dprintk(fmt, args...)   \
do {									\
	if (tsbuck_debug_log) {				\
		xlog_printk(ANDROID_LOG_INFO, "Power/PMIC_Thermal", fmt, ##args); \
	}								   \
} while(0)

/* Cali */
static kal_int32 g_o_vts = 0;
static kal_int32 g_degc_cali = 0;
static kal_int32 g_adc_cali_en = 0;
static kal_int32 g_o_slope = 0;
static kal_int32 g_o_slope_sign = 0;
static kal_int32 g_id = 0;
static kal_int32 g_slope1;
static kal_int32 g_slope2;
static kal_int32 g_intercept;
extern int PMIC_IMM_GetOneChannelValue(int dwChannel, int deCount, int trimd);
#define y_pmic_repeat_times	1



extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);


void tsbuck_read_6332_efuse(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int i=0,j=0;
	U32 efusevalue[2];

    printk("[tsbuck_read_6332_efuse] start\n");

    //1. enable efuse ctrl engine clock
    ret=pmic_config_interface(0x80B6, 0x0010, 0xFFFF, 0);
    ret=pmic_config_interface(0x80A4, 0x0004, 0xFFFF, 0);

    //2.
    ret=pmic_config_interface(0x8C6C, 0x1, 0x1, 0);
/*
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        0x80B2,upmu_get_reg_value(0x80B2),
        0x80A0,upmu_get_reg_value(0x80A0),
        0x8C6C,upmu_get_reg_value(0x8C6C)
        );
*/
//    for(i=0;i<=0x1F;i++)
    for(i=0x10;i<=0x11;i++)
    {
        //3. set row to read
        ret=pmic_config_interface(0x8C56, i, 0x1F, 1);

        //4. Toggle
        ret=pmic_read_interface(0x8C66, &reg_val, 0x1, 0);
        if(reg_val==0)
            ret=pmic_config_interface(0x8C66, 1, 0x1, 0);
        else
            ret=pmic_config_interface(0x8C66, 0, 0x1, 0);



        reg_val=1;
        while(reg_val == 1)
        {
            ret=pmic_read_interface(0x8C70, &reg_val, 0x1, 0);
            printk("5. polling Reg[0x61A][0]=0x%x\n", reg_val);
        }

		udelay(1000);//Need to delay at least 1ms for 0x8C70 and than can read 0x8C6E
        printk("5. 6332 delay 1 ms\n");

        //6. read data
        efusevalue[j] = upmu_get_reg_value(0x8C6E);
		printk("6332_efuse : efusevalue[%d]=0x%x\n",j, efusevalue[j]);
/*
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "i=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
            i,
            0x8C56,upmu_get_reg_value(0x8C56),
            0x8C70,upmu_get_reg_value(0x8C70),
            0x8C6E,upmu_get_reg_value(0x8C6E)
            );
*/
		j++;
    }

    //7. Disable efuse ctrl engine clock
    ret=pmic_config_interface(0x80A2, 0x0004, 0xFFFF, 0);
    ret=pmic_config_interface(0x80B4, 0x0010, 0xFFFF, 0); // new add
/*
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x\n",
        0x80A0,upmu_get_reg_value(0x80A0)
        );
*/

	g_adc_cali_en = (efusevalue[0])&0x1;
    g_degc_cali   = (efusevalue[0]>>1)&0x3F;
    g_o_vts       = ((efusevalue[0]>>7)&0x1FF) + (((efusevalue[1])&0xF)<<9);
	g_o_slope_sign= (efusevalue[1]>>4)&0x1;
    g_o_slope     = (efusevalue[1]>>5)&0x3F;
	g_id          = (efusevalue[1]>>11)&0x1;

	tsbuck_dprintk("tsbuck_read_6332_efuse: g_o_vts        = %x\n", g_o_vts);
    tsbuck_dprintk("tsbuck_read_6332_efuse: g_degc_cali    = %x\n", g_degc_cali);
    tsbuck_dprintk("tsbuck_read_6332_efuse: g_adc_cali_en  = %x\n", g_adc_cali_en);
    tsbuck_dprintk("tsbuck_read_6332_efuse: g_o_slope      = %x\n", g_o_slope);
    tsbuck_dprintk("tsbuck_read_6332_efuse: g_o_slope_sign = %x\n", g_o_slope_sign);
    tsbuck_dprintk("tsbuck_read_6332_efuse: g_id           = %x\n", g_id);

    tsbuck_dprintk("tsbuck_read_6332_efuse: ((efusevalue[0]>>7)&0x1FF) = 0x%x\n", ((efusevalue[0]>>7)&0x1FF));
    tsbuck_dprintk("tsbuck_read_6332_efuse: (((efusevalue[1])&0xF)<<9) = 0x%x\n", (((efusevalue[1])&0xF)<<9));

    tsbuck_dprintk("[tsbuck_read_6332_efuse] Done\n");
}




static void tsbuck_cali_prepare(void)
{

	tsbuck_read_6332_efuse();

	if(g_id==0)
	{
	   g_o_slope=0;
	}

	//g_adc_cali_en=0;//FIX ME

	if(g_adc_cali_en == 0) //no calibration
	{
		g_o_vts = 896;
		g_degc_cali = 50;
		g_o_slope = 0;
		g_o_slope_sign = 0;
	}
	printk("Power/BUCK_Thermal: g_o_vts        = 0x%x\n", g_o_vts);
    printk("Power/BUCK_Thermal: g_degc_cali    = 0x%x\n", g_degc_cali);
    printk("Power/BUCK_Thermal: g_adc_cali_en  = 0x%x\n", g_adc_cali_en);
    printk("Power/BUCK_Thermal: g_o_slope      = 0x%x\n", g_o_slope);
    printk("Power/BUCK_Thermal: g_o_slope_sign = 0x%x\n", g_o_slope_sign);
    printk("Power/BUCK_Thermal: g_id           = 0x%x\n", g_id);
}


static void tsbuck_cali_prepare2(void)
{
	kal_int32 vbe_t;
	g_slope1 = (100 * 1000);	//1000 is for 0.001 degree
	if(g_o_slope_sign==0)
	{
		g_slope2 = -(171+g_o_slope);
	}
	else
	{
		g_slope2 = -(171-g_o_slope);
	}
    vbe_t= (-1) * (((g_o_vts)*3200)/4096) * 1000;
	if(g_o_slope_sign==0)
	{
		g_intercept = (vbe_t * 100) / (-(171+g_o_slope)); 	//0.001 degree
	}
	else
	{
		g_intercept = (vbe_t * 100) / (-(171-g_o_slope));  //0.001 degree
	}
	g_intercept = g_intercept + (g_degc_cali*(1000/2)); // 1000 is for 0.1 degree
	printk("[Power/PMIC_Thermal] [Thermal calibration] SLOPE1=%d SLOPE2=%d INTERCEPT=%d, Vbe = %d\n",
		g_slope1, g_slope2, g_intercept,vbe_t);

}

static kal_int32 pmic_raw_to_temp(kal_uint32 ret)
{
	kal_int32 y_curr = ret;
	kal_int32 t_current;
	t_current = g_intercept + ((g_slope1 * y_curr) / (g_slope2));
    //tsbuck_dprintk("[pmic_raw_to_temp] t_current=%d\n",t_current);
	return t_current;
}


//int ts_pmic_at_boot_time=0;
static DEFINE_MUTEX(TSPMIC_lock);
static int pre_temp1=0, PMIC_counter=0;
static int tsbuck_get_hw_temp(void)
{
	int temp=0, temp1=0;
	//int temp3=0;
	mutex_lock(&TSPMIC_lock);



	temp = PMIC_IMM_GetOneChannelValue(ADC_TSENSE_32_AP, y_pmic_repeat_times , 2);
    temp1 = pmic_raw_to_temp(temp);
    //temp2 = pmic_raw_to_temp(675);

	tsbuck_dprintk("[tsbuck_get_hw_temp] Raw=%d, T=%d\n",temp, temp1);


	if((temp1>100000) || (temp1<-30000))
	{
		printk("[Power/PMIC_Thermal] raw=%d, PMIC T=%d", temp, temp1);
	}

	if((temp1>150000) || (temp1<-50000))
	{
		printk("[Power/PMIC_Thermal] drop this data\n");
		temp1 = pre_temp1;
	}
	else if( (PMIC_counter!=0) && (((pre_temp1-temp1)>30000) || ((temp1-pre_temp1)>30000)) )
	{
		printk("[Power/PMIC_Thermal] drop this data 2\n");
		temp1 = pre_temp1;
	}
	else
	{
		//update previous temp
		pre_temp1 = temp1;
		tsbuck_dprintk("[Power/PMIC_Thermal] pre_temp1=%d\n", pre_temp1);

		if(PMIC_counter==0)
			PMIC_counter++;
	}



	mutex_unlock(&TSPMIC_lock);
	return temp1;
}

static int tsbuck_get_temp(struct thermal_zone_device *thermal,
				           unsigned long *t)
{
	*t = tsbuck_get_hw_temp();

    if ((int) *t >= polling_trip_temp1) 
        thermal->polling_delay = interval*1000;
    else if ((int) *t < polling_trip_temp2)
        thermal->polling_delay = interval * polling_factor2;
    else
        thermal->polling_delay = interval * polling_factor1;

	return 0;
}

static int tsbuck_bind(struct thermal_zone_device *thermal,
			struct thermal_cooling_device *cdev)
{
	int table_val=0;

	if(!strcmp(cdev->type, g_bind0))
	{
		table_val = 0;
		tsbuck_dprintk("[tsbuck_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind1))
	{
		table_val = 1;
		tsbuck_dprintk("[tsbuck_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind2))
	{
		table_val = 2;
		tsbuck_dprintk("[tsbuck_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind3))
	{
		table_val = 3;
		tsbuck_dprintk("[tsbuck_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind4))
	{
		table_val = 4;
		tsbuck_dprintk("[tsbuck_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind5))
	{
		table_val = 5;
		tsbuck_dprintk("[tsbuck_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind6))
	{
		table_val = 6;
		tsbuck_dprintk("[tsbuck_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind7))
	{
		table_val = 7;
		tsbuck_dprintk("[tsbuck_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind8))
	{
		table_val = 8;
		tsbuck_dprintk("[tsbuck_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind9))
	{
		table_val = 9;
		tsbuck_dprintk("[tsbuck_bind] %s\n", cdev->type);
	}
	else
	{
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		tsbuck_dprintk("[tsbuck_bind] error binding cooling dev\n");
		return -EINVAL;
	} else {
		tsbuck_dprintk("[tsbuck_bind] binding OK, %d\n", table_val);
	}

	return 0;
}

static int tsbuck_unbind(struct thermal_zone_device *thermal,
			  struct thermal_cooling_device *cdev)
{
	int table_val=0;

	if(!strcmp(cdev->type, g_bind0))
	{
		table_val = 0;
		tsbuck_dprintk("[tsbuck_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind1))
	{
		table_val = 1;
		tsbuck_dprintk("[tsbuck_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind2))
	{
		table_val = 2;
		tsbuck_dprintk("[tsbuck_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind3))
	{
		table_val = 3;
		tsbuck_dprintk("[tsbuck_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind4))
	{
		table_val = 4;
		tsbuck_dprintk("[tsbuck_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind5))
	{
		table_val = 5;
		tsbuck_dprintk("[tsbuck_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind6))
	{
		table_val = 6;
		tsbuck_dprintk("[tsbuck_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind7))
	{
		table_val = 7;
		tsbuck_dprintk("[tsbuck_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind8))
	{
		table_val = 8;
		tsbuck_dprintk("[tsbuck_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind9))
	{
		table_val = 9;
		tsbuck_dprintk("[tsbuck_unbind] %s\n", cdev->type);
	}
	else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		tsbuck_dprintk("[tsbuck_unbind] error unbinding cooling dev\n");
		return -EINVAL;
	} else {
		tsbuck_dprintk("[tsbuck_unbind] unbinding OK\n");
	}

	return 0;
}

static int tsbuck_get_mode(struct thermal_zone_device *thermal,
				enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED
				 : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int tsbuck_set_mode(struct thermal_zone_device *thermal,
				enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int tsbuck_get_trip_type(struct thermal_zone_device *thermal, int trip,
				 enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int tsbuck_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				 unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int tsbuck_get_crit_temp(struct thermal_zone_device *thermal,
				 unsigned long *temperature)
{
	*temperature = TSBUCK_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops tsbuck_dev_ops = {
	.bind = tsbuck_bind,
	.unbind = tsbuck_unbind,
	.get_temp = tsbuck_get_temp,
	.get_mode = tsbuck_get_mode,
	.set_mode = tsbuck_set_mode,
	.get_trip_type = tsbuck_get_trip_type,
	.get_trip_temp = tsbuck_get_trip_temp,
	.get_crit_temp = tsbuck_get_crit_temp,
};

static int tspmic_sysrst_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	*state = 1;
	return 0;
}
static int tspmic_sysrst_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	*state = cl_dev_sysrst_state;
	return 0;
}
static int tspmic_sysrst_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	cl_dev_sysrst_state = state;
	if(cl_dev_sysrst_state == 1)
	{
		printk("Power/PMIC_Thermal: reset, reset, reset!!!");
		printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		printk("*****************************************");
		printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");

#ifndef CONFIG_ARM64
        BUG();
#else
        *(unsigned int*) 0x0 = 0xdead; // To trigger data abort to reset the system for thermal protection.
#endif
	}
	return 0;
}

static struct thermal_cooling_device_ops tsbuck_cooling_sysrst_ops = {
	.get_max_state = tspmic_sysrst_get_max_state,
	.get_cur_state = tspmic_sysrst_get_cur_state,
	.set_cur_state = tspmic_sysrst_set_cur_state,
};


static int tsbuck_read(struct seq_file *m, void *v)
{
	seq_printf(m, "[ tsbuck_read] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d,\n\
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

int tsbuck_register_thermal(void);
void tsbuck_unregister_thermal(void);

static ssize_t tsbuck_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
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
		tsbuck_dprintk("[tsbuck_write] tsbuck_unregister_thermal\n");
		tsbuck_unregister_thermal();

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

		tsbuck_dprintk("[tsbuck_write] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,\
g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
													g_THERMAL_TRIP[0],g_THERMAL_TRIP[1],g_THERMAL_TRIP[2],g_THERMAL_TRIP[3],g_THERMAL_TRIP[4],
													g_THERMAL_TRIP[5],g_THERMAL_TRIP[6],g_THERMAL_TRIP[7],g_THERMAL_TRIP[8],g_THERMAL_TRIP[9]);
		tsbuck_dprintk("[tsbuck_write] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\
cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
													g_bind0,g_bind1,g_bind2,g_bind3,g_bind4,g_bind5,g_bind6,g_bind7,g_bind8,g_bind9);

		for(i=0; i<num_trip; i++)
		{
			trip_temp[i]=trip[i];
		}

		interval=time_msec / 1000;

		tsbuck_dprintk("[tsbuck_write] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d,\
trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,time_ms=%d\n",
						trip_temp[0],trip_temp[1],trip_temp[2],trip_temp[3],trip_temp[4],
						trip_temp[5],trip_temp[6],trip_temp[7],trip_temp[8],trip_temp[9],interval*1000);

		tsbuck_dprintk("[tsbuck_write] tsbuck_register_thermal\n");
		tsbuck_register_thermal();

		return count;
	}
	else
	{
		tsbuck_dprintk("[tsbuck_write] bad argument\n");
	}

	return -EINVAL;
}

int tsbuck_register_cooler(void)
{
	cl_dev_sysrst = mtk_thermal_cooling_device_register("mtktsbuck-sysrst", NULL,
					   &tsbuck_cooling_sysrst_ops);
   	return 0;
}

int tsbuck_register_thermal(void)
{
	tsbuck_dprintk("[tsbuck_register_thermal] \n");

    /* trips : trip 0~2 */
    if (NULL == thz_dev) {
        thz_dev = mtk_thermal_zone_device_register("mtktsbuck", num_trip, NULL,
                                                   &tsbuck_dev_ops, 0, 0, 0, interval*1000);
    }

	return 0;
}

void tsbuck_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

void tsbuck_unregister_thermal(void)
{
	tsbuck_dprintk("[tsbuck_unregister_thermal] \n");

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int tsbuck_open(struct inode *inode, struct file *file)
{
	return single_open(file, tsbuck_read, NULL);
}

static const struct file_operations tsbuck_fops = {
	.owner = THIS_MODULE,
	.open = tsbuck_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tsbuck_write,
	.release = single_release,
};

static int __init tsbuck_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *tsbuck_dir = NULL;

	tsbuck_dprintk("[tsbuck_init] \n");
	tsbuck_cali_prepare();
	tsbuck_cali_prepare2();

	err = tsbuck_register_cooler();
	if(err)
		return err;
	err = tsbuck_register_thermal();
	if (err)
		goto err_unreg;

    tsbuck_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
    if (!tsbuck_dir)
    {
        tsbuck_dprintk("[%s]: mkdir /proc/driver/thermal failed\n", __func__);
    }
    else
    {
		entry = proc_create("tzbuck",  S_IRUGO | S_IWUSR | S_IWGRP, tsbuck_dir, &tsbuck_fops);
        if (entry) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
            proc_set_user(entry, 0, 1000);
#else
            entry->gid = 1000;
#endif
        }
    }

	return 0;

err_unreg:
		tsbuck_unregister_cooler();
		return err;
}

static void __exit tsbuck_exit(void)
{
	tsbuck_dprintk("[tsbuck_exit] \n");
	tsbuck_unregister_thermal();
	tsbuck_unregister_cooler();
}

module_init(tsbuck_init);
module_exit(tsbuck_exit);

