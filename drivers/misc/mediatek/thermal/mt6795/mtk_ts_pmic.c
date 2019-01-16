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
static int mtktspmic_debug_log = 0;
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

#define mtktspmic_TEMP_CRIT 150000 /* 150.000 degree Celsius */

#define mtktspmic_dprintk(fmt, args...)   \
do {									\
	if (mtktspmic_debug_log) {				\
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



void mtktspmic_read_6331_efuse(void);
extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);

void mtktspmic_read_6331_efuse(void)
{
    U32 ret=0;
    U32 reg_val=0;
    int i=0,j=0;
    U32 efusevalue[2];

    mtktspmic_dprintk("[mtktspmic_read_6331_efuse] start\n");

    //1. enable efuse ctrl engine clock
    ret=pmic_config_interface(0x0154, 0x0010, 0xFFFF, 0);
    ret=pmic_config_interface(0x0148, 0x0004, 0xFFFF, 0);

    //2.
    ret=pmic_config_interface(0x0616, 0x1, 0x1, 0);
/*
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        0x0154,upmu_get_reg_value(0x0154),
        0x0148,upmu_get_reg_value(0x0148),
        0x0616,upmu_get_reg_value(0x0616)
        );
*/
    for(i=0x14;i<=0x15;i++)
    {
        //3. set row to read
        ret=pmic_config_interface(0x0600, i, 0x1F, 1);

        //4. Toggle
        ret=pmic_read_interface(0x610, &reg_val, 0x1, 0);
        if(reg_val==0)
            ret=pmic_config_interface(0x610, 1, 0x1, 0);
        else
            ret=pmic_config_interface(0x610, 0, 0x1, 0);

        reg_val=1;
        while(reg_val == 1)
        {
            ret=pmic_read_interface(0x61A, &reg_val, 0x1, 0);
            mtktspmic_dprintk("5. polling Reg[0x61A][0]=0x%x\n", reg_val);
        }

		udelay(1000);//Need to delay at least 1ms for 0x61A and than can read 0x618
        printk("5. 6331 delay 1 ms\n");


        //6. read data
        efusevalue[j] = upmu_get_reg_value(0x0618);
        printk("6331_efuse : efusevalue[%d]=0x%x\n",j, efusevalue[j]);
/*
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "i=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
            i,
            0x0600,upmu_get_reg_value(0x0600),
            0x061A,upmu_get_reg_value(0x061A),
            0x0618,upmu_get_reg_value(0x0618)
            );
*/
        j++;
    }


    //7. Disable efuse ctrl engine clock
    ret=pmic_config_interface(0x0146, 0x0004, 0xFFFF, 0);
    ret=pmic_config_interface(0x0152, 0x0010, 0xFFFF, 0); // new add
    //dump
/*
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x\n",
        0x0144,upmu_get_reg_value(0x0144)
        );
*/

	g_adc_cali_en = (efusevalue[0]>>3)&0x1;
    g_degc_cali   = (efusevalue[0]>>4)&0x3F;
    g_o_vts       = ((efusevalue[0]>>10)&0x3F) + (((efusevalue[1])&0x7F)<<6);
	g_o_slope_sign= (efusevalue[1]>>7)&0x1;
    g_o_slope     = (efusevalue[1]>>8)&0x3F;
	g_id          = (efusevalue[1]>>14)&0x1;

	mtktspmic_dprintk("mtktspmic_read_6331_efuse: g_o_vts        = %x\n", g_o_vts);
    mtktspmic_dprintk("mtktspmic_read_6331_efuse: g_degc_cali    = %x\n", g_degc_cali);
    mtktspmic_dprintk("mtktspmic_read_6331_efuse: g_adc_cali_en  = %x\n", g_adc_cali_en);
    mtktspmic_dprintk("mtktspmic_read_6331_efuse: g_o_slope      = %x\n", g_o_slope);
    mtktspmic_dprintk("mtktspmic_read_6331_efuse: g_o_slope_sign = %x\n", g_o_slope_sign);
    mtktspmic_dprintk("mtktspmic_read_6331_efuse: g_id           = %x\n", g_id);

    mtktspmic_dprintk("mtktspmic_read_6331_efuse: ((efusevalue[0]>>10)&0x3F) = 0x%x\n", ((efusevalue[0]>>10)&0x3F));
    mtktspmic_dprintk("mtktspmic_read_6331_efuse: (((efusevalue[1])&0x7F)<<6) = 0x%x\n", (((efusevalue[1])&0x7F)<<6));

    mtktspmic_dprintk("[mtktspmic_read_6331_efuse] Done\n");
}



static void pmic_cali_prepare(void)
{
//	kal_uint32 temp0, temp1;

	mtktspmic_read_6331_efuse();

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

	printk("Power/PMIC_Thermal: g_o_vts        = 0x%x\n", g_o_vts);
    printk("Power/PMIC_Thermal: g_degc_cali    = 0x%x\n", g_degc_cali);
    printk("Power/PMIC_Thermal: g_adc_cali_en  = 0x%x\n", g_adc_cali_en);
    printk("Power/PMIC_Thermal: g_o_slope      = 0x%x\n", g_o_slope);
    printk("Power/PMIC_Thermal: g_o_slope_sign = 0x%x\n", g_o_slope_sign);
    printk("Power/PMIC_Thermal: g_id           = 0x%x\n", g_id);

}


static void pmic_cali_prepare2(void)
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
	mtktspmic_dprintk("[Power/PMIC_Thermal] [Thermal calibration] SLOPE1=%d SLOPE2=%d INTERCEPT=%d, Vbe = %d\n",
		g_slope1, g_slope2, g_intercept,vbe_t);

}

static kal_int32 pmic_raw_to_temp(kal_uint32 ret)
{
	kal_int32 y_curr = ret;
	kal_int32 t_current;
	t_current = g_intercept + ((g_slope1 * y_curr) / (g_slope2));
    mtktspmic_dprintk("[pmic_raw_to_temp] t_current=%d\n",t_current);
	return t_current;
}


static DEFINE_MUTEX(TSPMIC_lock);
static int pre_temp1=0, PMIC_counter=0;
static int mtktspmic_get_hw_temp(void)
{
	int temp=0, temp1=0;

	mutex_lock(&TSPMIC_lock);



	temp = PMIC_IMM_GetOneChannelValue(ADC_TSENSE_31_AP , y_pmic_repeat_times , 2);
    temp1 = pmic_raw_to_temp(temp);

	mtktspmic_dprintk("[mtktspmic_get_hw_temp]Raw=%d, T=%d\n",temp, temp1);


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
		mtktspmic_dprintk("[Power/PMIC_Thermal] pre_temp1=%d\n", pre_temp1);

		if(PMIC_counter==0)
			PMIC_counter++;
	}



	mutex_unlock(&TSPMIC_lock);
	return temp1;
}

static int mtktspmic_get_temp(struct thermal_zone_device *thermal,
				              unsigned long *t)
{
	*t = mtktspmic_get_hw_temp();

    if ((int) *t >= polling_trip_temp1) 
        thermal->polling_delay = interval*1000;
    else if ((int) *t < polling_trip_temp2)
        thermal->polling_delay = interval * polling_factor2;
    else
        thermal->polling_delay = interval * polling_factor1;
        
	return 0;
}

static int mtktspmic_bind(struct thermal_zone_device *thermal,
			struct thermal_cooling_device *cdev)
{
	int table_val=0;

	if(!strcmp(cdev->type, g_bind0))
	{
		table_val = 0;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind1))
	{
		table_val = 1;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind2))
	{
		table_val = 2;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind3))
	{
		table_val = 3;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind4))
	{
		table_val = 4;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind5))
	{
		table_val = 5;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind6))
	{
		table_val = 6;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind7))
	{
		table_val = 7;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind8))
	{
		table_val = 8;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind9))
	{
		table_val = 9;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	}
	else
	{
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtktspmic_dprintk("[mtktspmic_bind] error binding cooling dev\n");
		return -EINVAL;
	} else {
		mtktspmic_dprintk("[mtktspmic_bind] binding OK, %d\n", table_val);
	}

	return 0;
}

static int mtktspmic_unbind(struct thermal_zone_device *thermal,
			  struct thermal_cooling_device *cdev)
{
	int table_val=0;

	if(!strcmp(cdev->type, g_bind0))
	{
		table_val = 0;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind1))
	{
		table_val = 1;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind2))
	{
		table_val = 2;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind3))
	{
		table_val = 3;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind4))
	{
		table_val = 4;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind5))
	{
		table_val = 5;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind6))
	{
		table_val = 6;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind7))
	{
		table_val = 7;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind8))
	{
		table_val = 8;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind9))
	{
		table_val = 9;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	}
	else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtktspmic_dprintk("[mtktspmic_unbind] error unbinding cooling dev\n");
		return -EINVAL;
	} else {
		mtktspmic_dprintk("[mtktspmic_unbind] unbinding OK\n");
	}

	return 0;
}

static int mtktspmic_get_mode(struct thermal_zone_device *thermal,
				enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED
				 : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtktspmic_set_mode(struct thermal_zone_device *thermal,
				enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktspmic_get_trip_type(struct thermal_zone_device *thermal, int trip,
				 enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktspmic_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				 unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktspmic_get_crit_temp(struct thermal_zone_device *thermal,
				 unsigned long *temperature)
{
	*temperature = mtktspmic_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktspmic_dev_ops = {
	.bind = mtktspmic_bind,
	.unbind = mtktspmic_unbind,
	.get_temp = mtktspmic_get_temp,
	.get_mode = mtktspmic_get_mode,
	.set_mode = mtktspmic_set_mode,
	.get_trip_type = mtktspmic_get_trip_type,
	.get_trip_temp = mtktspmic_get_trip_temp,
	.get_crit_temp = mtktspmic_get_crit_temp,
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

static struct thermal_cooling_device_ops mtktspmic_cooling_sysrst_ops = {
	.get_max_state = tspmic_sysrst_get_max_state,
	.get_cur_state = tspmic_sysrst_get_cur_state,
	.set_cur_state = tspmic_sysrst_set_cur_state,
};


static int mtktspmic_read(struct seq_file *m, void *v)
{
	seq_printf(m, "[ mtktspmic_read] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d,\n\
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

int mtktspmic_register_thermal(void);
void mtktspmic_unregister_thermal(void);

static ssize_t mtktspmic_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
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
		mtktspmic_dprintk("[mtktspmic_write] mtktspmic_unregister_thermal\n");
		mtktspmic_unregister_thermal();

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

		mtktspmic_dprintk("[mtktspmic_write] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,\
g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
													g_THERMAL_TRIP[0],g_THERMAL_TRIP[1],g_THERMAL_TRIP[2],g_THERMAL_TRIP[3],g_THERMAL_TRIP[4],
													g_THERMAL_TRIP[5],g_THERMAL_TRIP[6],g_THERMAL_TRIP[7],g_THERMAL_TRIP[8],g_THERMAL_TRIP[9]);
		mtktspmic_dprintk("[mtktspmic_write] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\
cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
													g_bind0,g_bind1,g_bind2,g_bind3,g_bind4,g_bind5,g_bind6,g_bind7,g_bind8,g_bind9);

		for(i=0; i<num_trip; i++)
		{
			trip_temp[i]=trip[i];
		}

		interval=time_msec / 1000;

		mtktspmic_dprintk("[mtktspmic_write] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d,\
trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,time_ms=%d\n",
						trip_temp[0],trip_temp[1],trip_temp[2],trip_temp[3],trip_temp[4],
						trip_temp[5],trip_temp[6],trip_temp[7],trip_temp[8],trip_temp[9],interval*1000);

		mtktspmic_dprintk("[mtktspmic_write] mtktspmic_register_thermal\n");
		mtktspmic_register_thermal();

		return count;
	}
	else
	{
		mtktspmic_dprintk("[mtktspmic_write] bad argument\n");
	}

	return -EINVAL;
}

int mtktspmic_register_cooler(void)
{
	cl_dev_sysrst = mtk_thermal_cooling_device_register("mtktspmic-sysrst", NULL,
					   &mtktspmic_cooling_sysrst_ops);
   	return 0;
}

int mtktspmic_register_thermal(void)
{
	mtktspmic_dprintk("[mtktspmic_register_thermal] \n");

    /* trips : trip 0~2 */
    if (NULL == thz_dev) {
        thz_dev = mtk_thermal_zone_device_register("mtktspmic", num_trip, NULL,
                                                   &mtktspmic_dev_ops, 0, 0, 0, interval*1000);
    }

	return 0;
}

void mtktspmic_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

void mtktspmic_unregister_thermal(void)
{
	mtktspmic_dprintk("[mtktspmic_unregister_thermal] \n");

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtktspmic_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktspmic_read, NULL);
}

static const struct file_operations mtktspmic_fops = {
	.owner = THIS_MODULE,
	.open = mtktspmic_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtktspmic_write,
	.release = single_release,
};

static int __init mtktspmic_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktspmic_dir = NULL;

	mtktspmic_dprintk("[mtktspmic_init] \n");
	pmic_cali_prepare();
	pmic_cali_prepare2();

	err = mtktspmic_register_cooler();
	if(err)
		return err;
	err = mtktspmic_register_thermal();
	if (err)
		goto err_unreg;

	mtktspmic_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktspmic_dir)
	{
		mtktspmic_dprintk("[%s]: mkdir /proc/driver/thermal failed\n", __func__);
	}
	else
	{
        entry = proc_create("tzpmic", S_IRUGO | S_IWUSR | S_IWGRP, mtktspmic_dir, &mtktspmic_fops);
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
		mtktspmic_unregister_cooler();
		return err;
}

static void __exit mtktspmic_exit(void)
{
	mtktspmic_dprintk("[mtktspmic_exit] \n");
	mtktspmic_unregister_thermal();
	mtktspmic_unregister_cooler();
}

module_init(mtktspmic_init);
module_exit(mtktspmic_exit);
