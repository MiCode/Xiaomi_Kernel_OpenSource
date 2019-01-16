

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

//#include <mach/mt_devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>


#include <asm/io.h>
#include <cust_eint.h>

#include <linux/input.h>

#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#define HALL_PROC_FILE "hall_status"

static u16 nEnableKey = 1;
/******************************************************************************
 * configuration
*******************************************************************************/
/*----------------------------------------------------------------------------*/

#define HALL_TAG		"[ah1903] "
#define HALL_DEBUG
#if defined(HALL_DEBUG)
#define HALL_FUN(f)				printk(KERN_ERR HALL_TAG"%s\n", __FUNCTION__)
#define HALL_ERR(fmt, args...)		printk(KERN_ERR HALL_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#else
#define HALL_FUN(f)
#define HALL_ERR(fmt, args...)
#endif


/******************************************************************************
 * extern functions
*******************************************************************************/
extern void mt_eint_mask(unsigned int eint_num);
extern void mt_eint_unmask(unsigned int eint_num);
extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern void mt_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern unsigned int mt_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);



static atomic_t at_suspend;
static atomic_t	 at_close;	// 0 close Low;  1 leave High;
static struct mutex mtx_eint_status;

static struct work_struct  eint_work;
static struct input_dev *hall_kpd = NULL;
static struct early_suspend    early_drv;
/*----------------------------------------------------------------------------*/

static void ah1903_eint_work(struct work_struct *work);

/*----------------------------------------------------------------------------*/
static void ah1903_power(unsigned int on) 
{
	if(on)
	{
		//hwPowerOn(MT65XX_POWER_LDO_VGP4, VOL_2800, "hall");
	}
	else
	{
		//hwPowerDown(MT65XX_POWER_LDO_VGP4, "hall")
	}
}

static struct proc_dir_entry *hall_status_proc = NULL;
static int hall_proc_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	char *page = NULL;
	char *ptr = NULL;
	int len, err = -1;
	unsigned char status;

	HALL_ERR("Enter hall_proc_read\n");

	page = kmalloc(PAGE_SIZE, GFP_KERNEL);	
	if (!page) 
	{		
		HALL_ERR("Enter hall_proc_read, !page\n");
		kfree(page);		
		return -ENOMEM;	
	}
	
	ptr = page; 

	mutex_lock(&mtx_eint_status);
	status = atomic_read(&at_close);
	if(status == 1)
	{
		ptr += sprintf(page, "on\n");
	}
	else
	{
		ptr += sprintf(page, "off\n");
	}
	mutex_unlock(&mtx_eint_status);
	
	len = ptr - page; 			 	
	if(*ppos >= len)
	{	
		HALL_ERR("Enter ctp_proc_read, *ppos >= len\n");
		kfree(page); 		
		return 0; 	
	}	
	err = copy_to_user(buffer, (char *)page, len); 			
	*ppos += len; 
	
	if(err) 
	{		
		HALL_ERR("Enter ctp_proc_read, err\n");
		kfree(page); 		
		return err; 	
	}	
	
	kfree(page); 	
	return len;	
}

static const struct file_operations hall_proc_fops = 
{
	.read = hall_proc_read,
};

int ah1903_create_proc()
{
	hall_status_proc = proc_create(HALL_PROC_FILE, 0644, NULL, &hall_proc_fops);
	if (hall_status_proc == NULL)
	{
		HALL_ERR("tpd, create_proc_entry hall_status_proc failed\n");
	}

	return 0;	
}

void ah1903_eint_func(void)
{
	mt_eint_mask(CUST_EINT_MHALL_NUM);  
	ah1903_eint_work(&eint_work);
}

#define CUST_EINT_POLARITY_LOW              0
#define CUST_EINT_POLARITY_HIGH             1

int ah1903_setup_eint(void)
{
	unsigned char eint_bit;
	mutex_lock(&mtx_eint_status);
	mt_set_gpio_dir(GPIO_MHALL_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_mode(GPIO_MHALL_EINT_PIN, GPIO_MHALL_EINT_PIN_M_EINT);
	mt_set_gpio_pull_enable(GPIO_MHALL_EINT_PIN, FALSE);
	eint_bit = mt_get_gpio_in(GPIO_MHALL_EINT_PIN);	
	mt_eint_set_hw_debounce(CUST_EINT_MHALL_NUM, CUST_EINT_MHALL_DEBOUNCE_CN);
	if(eint_bit == 0)
	{
		atomic_set(&at_close, 1); // low close
		mt_eint_registration(CUST_EINT_MHALL_NUM, CUST_EINT_POLARITY_HIGH, ah1903_eint_func, 1);
	}
	else
	{
		atomic_set(&at_close, 0); // low close
		mt_eint_registration(CUST_EINT_MHALL_NUM, CUST_EINTF_TRIGGER_LOW, ah1903_eint_func, 1);
	}
	mt_eint_mask(CUST_EINT_MHALL_NUM); 
	mutex_unlock(&mtx_eint_status); 
	return 0;
}

static void ah1903_report_key(int value)
{
	if(nEnableKey)
	{
		input_report_switch(hall_kpd, SW_LID, value);
		input_sync(hall_kpd);	
	}
}

static void ah1903_eint_work(struct work_struct *work)
{
	unsigned char eint_bit;
	unsigned char last_status;
	
	last_status = atomic_read(&at_close);
	if(hall_kpd != NULL)
	{
		HALL_ERR("[hall] ah1903_eint_work last_status=%d\n", last_status);
		if(last_status)	// wake up the phone 
		{
			ah1903_report_key(0);
	  	}
	  	else		//sleep the phone
		{
			ah1903_report_key(1);
		}
	}
	
	// double check for set eint
	eint_bit = mt_get_gpio_in(GPIO_MHALL_EINT_PIN);	
	// low close
	if(eint_bit == last_status)
	{
		HALL_ERR("[hall] dismiss status;\n");
	}
	
	if(eint_bit == 0)
	{
		atomic_set(&at_close, 1); // low close
		mt_eint_set_polarity(CUST_EINT_MHALL_NUM, CUST_EINT_POLARITY_HIGH);
	}
	else
	{
		atomic_set(&at_close, 0); // low close
		mt_eint_set_polarity(CUST_EINT_MHALL_NUM, CUST_EINT_POLARITY_LOW);
	}
}

int ah1903_setup_input(void)
{
	int ret;
	HALL_ERR("[hall]: %s\n", __func__);
	hall_kpd = input_allocate_device();
	hall_kpd->name = "HALL";
	hall_kpd->id.bustype = BUS_HOST;

	__set_bit(EV_SW, hall_kpd->evbit);
	__set_bit(SW_LID,  hall_kpd->swbit );

	ret = input_register_device(hall_kpd);
	if(ret)
	{
		HALL_ERR("[hall]: %s register inputdevice failed\n", __func__);
	}
	
	return 0;
}

static void ah1903_early_suspend(struct early_suspend *h) 
{
	HALL_ERR("[hall]: %s \n", __func__);
	atomic_set(&at_suspend, 1);
	return;
}

static void ah1903_late_resume(struct early_suspend *h)
{
	unsigned char eint_bit;
	atomic_set(&at_suspend, 0);
	eint_bit = mt_get_gpio_in(GPIO_MHALL_EINT_PIN);	
	HALL_ERR("eint_bit  = %d\n", eint_bit);
	if(eint_bit == 0)
	{
		atomic_set(&at_close, 1); // low close
		mt_eint_set_polarity(CUST_EINT_MHALL_NUM, CUST_EINT_POLARITY_HIGH);
	}
	else
	{
		atomic_set(&at_close, 0); // low close
		mt_eint_set_polarity(CUST_EINT_MHALL_NUM, CUST_EINT_POLARITY_LOW);
	}

	return;
}

int ah1903_setup_earlySuspend(void)
{
	#if defined(CONFIG_HAS_EARLYSUSPEND)
	//early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	early_drv.level    = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1,
	early_drv.suspend  = ah1903_early_suspend,
	early_drv.resume   = ah1903_late_resume,    
	register_early_suspend(&early_drv);
	#endif

	return 0;
}

static int ah1903_probe(struct platform_device *pdev) 
{
	HALL_ERR("[hall]: %s \n", __func__);

	mt_set_gpio_mode(GPIO_MHALL_ENABLE_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_MHALL_ENABLE_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_MHALL_ENABLE_PIN, GPIO_OUT_ONE);
	
	INIT_WORK(&eint_work, ah1903_eint_work);

	atomic_set(&at_suspend, 0);
	
	mutex_init(&mtx_eint_status);
	ah1903_setup_eint();

	ah1903_setup_input();
	
	ah1903_setup_earlySuspend();

	mt_eint_unmask(CUST_EINT_MHALL_NUM);  
	
	ah1903_create_proc();
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int ah1903_remove(struct platform_device *pdev)
{
	HALL_ERR("[hall]: %s \n", __func__);
	return 0;
}
/*----------------------------------------------------------------------------*/
static struct platform_driver ah1903_driver = {
	.probe      = ah1903_probe,
	.remove     = ah1903_remove,    
	.driver     = {
		.name  = "ah1903",
//		.owner = THIS_MODULE,
	}
};
/*----------------------------------------------------------------------------*/


static struct platform_device ah1903_device = {
	.name = "ah1903"
};

static int __init ah1903_init(void)
{
	int retval;
	HALL_ERR("[hall]: %s \n", __func__);

	retval = platform_device_register(&ah1903_device);
	if (retval != 0)
	{
		HALL_ERR("[hall]: %s failed to register ah1903 device\n", __func__);
		return retval;
	}
	
	if(platform_driver_register(&ah1903_driver))
	{
		HALL_ERR("[hall] failed to register driver");
		return -ENODEV;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit ah1903_exit(void)
{
	platform_driver_unregister(&ah1903_driver);
}
/*----------------------------------------------------------------------------*/
module_init(ah1903_init);
module_exit(ah1903_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("longcheer");
MODULE_DESCRIPTION("BU52031NVX driver");
MODULE_LICENSE("GPL");
