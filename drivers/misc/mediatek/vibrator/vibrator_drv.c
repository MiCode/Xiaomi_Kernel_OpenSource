/******************************************************************************
 * mt6575_vibrator.c - MT6575 Android Linux Vibrator Device Driver
 *
 * Copyright 2009-2010 MediaTek Co.,Ltd.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * DESCRIPTION:
 *     This file provid the other drivers vibrator relative functions
 *
 ******************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>

#include "timed_output.h"

#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/delay.h>

#include <mach/mt_typedefs.h>
/* #include <mach/mt6577_pm_ldo.h> */

#include <cust_vibrator.h>
#include <vibrator_hal.h>

#include <mach/mt_pwm.h>

#define VERSION					        "v 0.1"
#define VIB_DEVICE				"mtk_vibrator"


/******************************************************************************
Error Code No.
******************************************************************************/
#define RSUCCESS        0

/******************************************************************************
Debug Message Settings
******************************************************************************/

/* Debug message event */
#define DBG_EVT_NONE		0x00000000	/* No event */
#define DBG_EVT_INT			0x00000001	/* Interrupt related event */
#define DBG_EVT_TASKLET		0x00000002	/* Tasklet related event */

#define DBG_EVT_ALL			0xffffffff

#define DBG_EVT_MASK		(DBG_EVT_TASKLET)

#if 1
#define MSG(evt, fmt, args...) \
do {	\
	if ((DBG_EVT_##evt) & DBG_EVT_MASK) { \
		pr_info(fmt, ##args); \
	} \
} while (0)

#define MSG_FUNC_ENTRY(f)	MSG(FUC, "<FUN_ENT>: %s\n", __func__)
#else
#define MSG(evt, fmt, args...) do {} while (0)
#define MSG_FUNC_ENTRY(f)	   do {} while (0)
#endif


/******************************************************************************
Global Definations
******************************************************************************/
static struct workqueue_struct *vibrator_queue;
static struct work_struct vibrator_work;
static struct hrtimer vibe_timer;
static spinlock_t vibe_lock;
static int vibe_state;
static int ldo_state=0;
static int shutdown_flag;

extern unsigned int g_call_state;
static int force_backup=500;
static void mt_vibrator_set_pwm_new(int force)
{
    struct pwm_spec_config pwm_setting;
    int temp_force=0;
    int THRESH=1160;  

	pwm_setting.pwm_no = PWM3;
	pwm_setting.mode = PWM_MODE_OLD;
	pwm_setting.clk_src = PWM_CLK_OLD_MODE_BLOCK;
    pwm_setting.pmic_pad=0;
	pwm_setting.clk_div = CLK_DIV1;
	
        
    temp_force=force+128;  //1-255  128=50%   0-100

    if(temp_force==128)
    {
        //THRESH=1478;
        // THRESH=1318;
        THRESH=1160/2;
    }else  if(temp_force>=255)
    {
        //THRESH=2956;
        //THRESH=2600;
        //THRESH=2452;
        THRESH=1150;
    }else if(temp_force<128)
    {
        //1 --   0
        //127 -- 49
        //2956 1478/127=11.63
        //2636  1318/127=10.37
        //THRESH=(1163*temp_force)/100;
        if(temp_force <= 1) 
        {
            THRESH=15;  //1%       
        }else
        {
            THRESH=(580*temp_force)/128;  
        }

    }else if(temp_force>128)
    {
        //128 --   51
        //255 -- 100
        //1478/127=11.63
        //2636  1318/127=10.37
        //2476   1238/127=9.748
        //1160    580/127=4.57
        THRESH=((457*(temp_force-128))/100)+580;
        if(THRESH>1150)
        {
           THRESH=1150;
        }
              
    }
    pr_info("[vibrator]temp_force,THRESH=%d \n",THRESH);
    pwm_setting.PWM_MODE_OLD_REGS.THRESH = THRESH;
    pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH = 1160;
	// 50% = 1478 
	// 100% = 2956
	// force = 1478+force*(2956-1478)/128
	pwm_setting.PWM_MODE_FIFO_REGS.IDLE_VALUE = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.GUARD_VALUE = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.GDURATION = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;

    mt_set_gpio_mode(GPIO_VIBRATOR_PWM_PIN, GPIO_MODE_03);
    udelay(10);
	pwm_set_spec_config(&pwm_setting);
    //mt_pwm_dump_regs();
    udelay(10);

             
        mt_set_gpio_mode(GPIO_VIBRATOR_POWER_EN_PIN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_VIBRATOR_POWER_EN_PIN,GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_VIBRATOR_POWER_EN_PIN,GPIO_OUT_ONE);
		udelay(10);
                   
		mt_set_gpio_mode(GPIO_VIBRATOR_EN_PIN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_VIBRATOR_EN_PIN,GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_VIBRATOR_EN_PIN,GPIO_OUT_ONE);

}

static void mt_vibrator_set_pwm(void)
{
	struct pwm_spec_config pwm_setting;
	pwm_setting.pwm_no = PWM3;
	pwm_setting.mode = PWM_MODE_OLD;
	pwm_setting.clk_src = PWM_CLK_OLD_MODE_BLOCK;
        pwm_setting.pmic_pad=0;
	pwm_setting.clk_div = CLK_DIV1;
	pr_info(KERN_INFO "[vibrator] mt_vibrator_set_pwm enter, ldo_state =  %d \n", ldo_state );
	if(ldo_state){
		pwm_setting.PWM_MODE_OLD_REGS.THRESH = 2452;
        }else{
		pwm_setting.PWM_MODE_OLD_REGS.THRESH = 0;
	}
        pwm_setting.PWM_MODE_OLD_REGS.DATA_WIDTH = 2476;
	pwm_setting.PWM_MODE_FIFO_REGS.IDLE_VALUE = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.GUARD_VALUE = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.GDURATION = 0;
	pwm_setting.PWM_MODE_FIFO_REGS.WAVE_NUM = 0;
	pwm_set_spec_config(&pwm_setting);
}


static int vibr_Enable(void)
{
#ifdef CONFIG_CM865_MAINBOARD //modify longcheer_liml_0922
	if (!ldo_state)
	{
		ldo_state=1;	
		vibr_Enable_HW();
		#if 0			
		vibr_Enable_HW();
		if(g_call_state == 40)
		{
		udelay(10*1200);
		vibr_Disable_HW();
		}
		else if(g_call_state == 80)
		{
		udelay(10*1500);
		vibr_Disable_HW();
		}
		else if(g_call_state == 120)
		{
		udelay(10*1100);
		 udelay(10*1100);
		vibr_Disable_HW();
		}
		#endif
	}

#else
    if (!ldo_state)
	{
		ldo_state=1;
		mt_vibrator_set_pwm_new(90);//120 
	}
#endif

	return 0;
}

static int vibr_Disable(void)
{
#ifdef CONFIG_CM865_MAINBOARD //modify longcheer_liml_0922

	if (ldo_state) 
	{
       // mt_vibrator_set_pwm_new(0-g_call_state);
       //udelay(30);
		vibr_Disable_HW();
		ldo_state=0;
	}
#else
	if (ldo_state) 
	{
		vibr_Disable_HW();
		ldo_state=0;
	    mt_set_gpio_out(GPIO_VIBRATOR_EN_PIN,GPIO_OUT_ZERO);
        udelay(10);
        mt_set_gpio_mode(GPIO_VIBRATOR_POWER_EN_PIN, GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_VIBRATOR_POWER_EN_PIN,GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_VIBRATOR_POWER_EN_PIN,GPIO_OUT_ZERO);
        udelay(10);
        mt_pwm_disable(PWM3, 0);
        mt_set_gpio_mode(GPIO_VIBRATOR_PWM_PIN, GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_VIBRATOR_PWM_PIN,GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_VIBRATOR_PWM_PIN,GPIO_OUT_ZERO);   
	}
#endif



	return 0;
}


static void update_vibrator(struct work_struct *work)
{
	if (!vibe_state)
		vibr_Disable();
	else
		vibr_Enable();
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	if (hrtimer_active(&vibe_timer)) {
		ktime_t r = hrtimer_get_remaining(&vibe_timer);
		return ktime_to_ms(r);
	} else
		return 0;
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	unsigned long flags;


#if 1
	/* struct vibrator_hw* hw = get_cust_vibrator_hw(); */
	struct vibrator_hw *hw = mt_get_cust_vibrator_hw();

#endif
	pr_info("[vibrator]vibrator_enable: vibrator first in value = %d\n", value);

	spin_lock_irqsave(&vibe_lock, flags);
	while (hrtimer_cancel(&vibe_timer)) {
		pr_info("[vibrator]vibrator_enable: try to cancel hrtimer\n");
	}

	if (value == 0 || shutdown_flag == 1) {
		pr_info("[vibrator]vibrator_enable: shutdown_flag = %d\n", shutdown_flag);
		vibe_state = 0;
	} else {
#if 1
		pr_info("[vibrator]vibrator_enable: vibrator cust timer: %d\n", hw->vib_timer);
#ifdef CUST_VIBR_LIMIT
		if (value > hw->vib_limit && value < hw->vib_timer)
#else
		if (value >= 10 && value < hw->vib_timer)
#endif
			value = hw->vib_timer;
#endif

		value = (value > 15000 ? 15000 : value);
		vibe_state = 1;
		hrtimer_start(&vibe_timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000), HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&vibe_lock, flags);
	pr_info("[vibrator]vibrator_enable: vibrator start: %d\n", value);
	queue_work(vibrator_queue, &vibrator_work);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	vibe_state = 0;
	pr_info(KERN_DEBUG "[vibrator]vibrator_timer_func: vibrator will disable\n");
	queue_work(vibrator_queue, &vibrator_work);
	return HRTIMER_NORESTART;
}

static struct timed_output_dev mtk_vibrator = {
	.name = "vibrator",
	.get_time = vibrator_get_time,
	.enable = vibrator_enable,
};

static int vib_probe(struct platform_device *pdev)
{
	return 0;
}

static int vib_remove(struct platform_device *pdev)
{
	return 0;
}

static void vib_shutdown(struct platform_device *pdev)
{
	unsigned long flags;
	pr_info("[vibrator]vib_shutdown: enter!\n");
	spin_lock_irqsave(&vibe_lock, flags);
	shutdown_flag = 1;
	if (vibe_state) {
		pr_info("[vibrator]vib_shutdown: vibrator will disable\n");
		vibe_state = 0;
		vibr_Disable();
	}
	spin_unlock_irqrestore(&vibe_lock, flags);
}

/******************************************************************************
Device driver structure
*****************************************************************************/
static struct platform_driver vibrator_driver = {
	.probe = vib_probe,
	.remove = vib_remove,
	.shutdown = vib_shutdown,
	.driver = {
		   .name = VIB_DEVICE,
		   .owner = THIS_MODULE,
		   },
};

static struct platform_device vibrator_device = {
	.name = "mtk_vibrator",
	.id = -1,
};

static ssize_t store_vibr_on(struct device *dev, struct device_attribute *attr, const char *buf,
			     size_t size)
{
	if (buf != NULL && size != 0) {
		//pr_info("[vibrator]buf is %s and size is %d\n", buf, size);
		if (buf[0] == '0') {
			vibr_Disable();
		} else {
			vibr_Enable();
		}
	}
	return size;
}

static DEVICE_ATTR(vibr_on, 0220, NULL, store_vibr_on);

/******************************************************************************
 * vib_mod_init
 *
 * DESCRIPTION:
 *   Register the vibrator device driver !
 *
 * PARAMETERS:
 *   None
 *
 * RETURNS:
 *   None
 *
 * NOTES:
 *   RSUCCESS : Success
 *
 ******************************************************************************/

static int vib_mod_init(void)
{
	s32 ret;

	pr_info("MediaTek MTK vibrator driver register, version %s\n", VERSION);
	vibr_power_set();	/* set vibr voltage if needs.  Before MT6320 vibr default voltage=2.8v but in MT6323 vibr default voltage=1.2v */
	ret = platform_device_register(&vibrator_device);
	if (ret != 0) {
		pr_info("[vibrator]Unable to register vibrator device (%d)\n", ret);
		return ret;
	}

	vibrator_queue = create_singlethread_workqueue(VIB_DEVICE);
	if (!vibrator_queue) {
		pr_info("[vibrator]Unable to create workqueue\n");
		return -ENODATA;
	}
	INIT_WORK(&vibrator_work, update_vibrator);

	spin_lock_init(&vibe_lock);
	shutdown_flag = 0;
	vibe_state = 0;
	hrtimer_init(&vibe_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibe_timer.function = vibrator_timer_func;

	timed_output_dev_register(&mtk_vibrator);

	ret = platform_driver_register(&vibrator_driver);

	if (ret) {
		pr_info("[vibrator]Unable to register vibrator driver (%d)\n", ret);
		return ret;
	}

	ret = device_create_file(mtk_vibrator.dev, &dev_attr_vibr_on);
	if (ret) {
		pr_info("[vibrator]device_create_file vibr_on fail!\n");
	}

	pr_info("[vibrator]vib_mod_init Done\n");

	return RSUCCESS;
}

/******************************************************************************
 * vib_mod_exit
 *
 * DESCRIPTION:
 *   Free the device driver !
 *
 * PARAMETERS:
 *   None
 *
 * RETURNS:
 *   None
 *
 * NOTES:
 *   None
 *
 ******************************************************************************/

static void vib_mod_exit(void)
{
	pr_info("MediaTek MTK vibrator driver unregister, version %s\n", VERSION);
	if (vibrator_queue) {
		destroy_workqueue(vibrator_queue);
	}
	pr_info("[vibrator]vib_mod_exit Done\n");
}
module_init(vib_mod_init);
module_exit(vib_mod_exit);
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MTK Vibrator Driver (VIB)");
MODULE_LICENSE("GPL");
