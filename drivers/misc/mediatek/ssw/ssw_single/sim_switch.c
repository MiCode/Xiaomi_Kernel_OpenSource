
#include <ssw.h>
#include <mach/mtk_ccci_helper.h>
/*--------------Feature option---------------*/
#define __ENABLE_SSW_SYSFS 1


/*--------------SIM mode list----------------*/
#define SINGLE_TALK_MDSYS				(0x1)
#define SINGLE_TALK_MDSYS_LITE			(0x2)
#define DUAL_TALK						(0x3)
#define DUAL_TALK_SWAP					(0x4)	

/*----------------Error Code-----------------*/
#define SSW_SUCCESS 					(0)
#define SSW_INVALID_PARA				(-1)


/*--------------Global varible---------------*/
unsigned int sim_mode_curr = SINGLE_TALK_MDSYS;

unsigned int get_sim_switch_type(void)
{
	printk("[ccci/ssw]SSW_GENERIC\n");
	return SSW_INTERN;
}
EXPORT_SYMBOL(get_sim_switch_type);

struct mutex sim_switch_mutex;


static inline void sim_switch_writel(void *addr, unsigned offset, u32 data)
{
	*((volatile unsigned int*)(addr + offset)) = data;
}

static inline u32 sim_switch_readl(const void *addr, unsigned offset)
{

	u32 rc = 0;
	rc = *((volatile unsigned int*)(addr + offset));
	return rc;
}

static int set_sim_gpio(unsigned int mode);
static int get_current_ssw_mode(void);

/*---------------------------------------------------------------------------*/
/*define sysfs entry for configuring debug level and sysrq*/
ssize_t ssw_attr_show(struct kobject *kobj, struct attribute *attr, char *buffer);
ssize_t ssw_attr_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size);
ssize_t ssw_mode_show(struct kobject *kobj, char *page);
ssize_t ssw_mode_store(struct kobject *kobj, const char *page, size_t size);

struct sysfs_ops ssw_sysfs_ops = {
	.show   = ssw_attr_show,
	.store  = ssw_attr_store,
};

struct ssw_sys_entry {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj, char *page);
	ssize_t (*store)(struct kobject *kobj, const char *page, size_t size);
};

static struct ssw_sys_entry mode_entry = {
	{ .name = "mode", .mode = S_IRUGO | S_IWUSR }, // remove  .owner = NULL,  
	ssw_mode_show,
	ssw_mode_store,
};

struct attribute *ssw_attributes[] = {
	&mode_entry.attr,
	NULL,
};

struct kobj_type ssw_ktype = {
	.sysfs_ops = &ssw_sysfs_ops,
	.default_attrs = ssw_attributes,
};

static struct ssw_sysobj_t {
	struct kobject kobj;
} ssw_sysobj;


int ssw_sysfs_init(void) 
{
	struct ssw_sysobj_t *obj = &ssw_sysobj;

	memset(&obj->kobj, 0x00, sizeof(obj->kobj));
    
	obj->kobj.parent = kernel_kobj;
	if (kobject_init_and_add(&obj->kobj, &ssw_ktype, NULL, "mtk_ssw")) {
		kobject_put(&obj->kobj);
		return -ENOMEM;
	}
	kobject_uevent(&obj->kobj, KOBJ_ADD);

	return 0;
}

ssize_t ssw_attr_show(struct kobject *kobj, struct attribute *attr, char *buffer) 
{
	struct ssw_sys_entry *entry = container_of(attr, struct ssw_sys_entry, attr);
	return entry->show(kobj, buffer);
}

ssize_t ssw_attr_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size) 
{
	struct ssw_sys_entry *entry = container_of(attr, struct ssw_sys_entry, attr);
	return entry->store(kobj, buffer, size);
}

ssize_t ssw_mode_show(struct kobject *kobj, char *buffer) 
{
	int remain = PAGE_SIZE;
	int len;
	char *ptr = buffer;

	len = scnprintf(ptr, remain, "0x%x\n", get_current_ssw_mode());
	ptr += len;
	remain -= len;
	SSW_DBG("ssw_mode_show\n");

	return (PAGE_SIZE-remain);
}

ssize_t ssw_mode_store(struct kobject *kobj, const char *buffer, size_t size) 
{
	int mode;
	int res = sscanf(buffer, "%x", &mode);

	if (res != 1)
	{
		printk("%s: expect 1 numbers\n", __FUNCTION__);
	}
	else
	{
		SSW_DBG("ssw_mode_store %d\n", mode);
		//Switch sim mode
		if ((sim_mode_curr != mode) && (SSW_SUCCESS == set_sim_gpio(mode)))
		{
			sim_mode_curr = mode;
		}
	}
	return size;
}
/*---------------------------------------------------------------------------*/



int get_current_ssw_mode(void)
{
	return sim_mode_curr;
}


static int set_sim_gpio(unsigned int mode)
{
	SSW_DBG("set_sim_gpio %d\n", mode);
	
	switch(mode)
	{
		case SINGLE_TALK_MDSYS:
			#if defined(GPIO_SIM1_SCLK) && defined(GPIO_SIM1_SIO) && defined(GPIO_SIM2_SCLK) && defined(GPIO_SIM2_SIO)
			mt_set_gpio_mode(GPIO_SIM1_SCLK, 1);	//SIM1_SCLK	
			mt_set_gpio_mode(GPIO_SIM1_SIO, 1); 	//SIM1_SIO
			mt_set_gpio_mode(GPIO_SIM2_SCLK, 1); 	//SIM2_SCLK
			mt_set_gpio_mode(GPIO_SIM2_SIO, 1); 	//SIM2_SIO
			//mt_set_gpio_mode(GPIO_SIM1_SRST, 4);	//SIM1_SRST, 6582 not use reset pin
			//mt_set_gpio_mode(GPIO_SIM2_SRST, 4);	//SIM2_SRST, 6582 not use reset pin
			#endif
			break;
		
		default:
			SSW_DBG("Mode(%d) not supported!!!", mode);
			return SSW_INVALID_PARA;
	}

#if 0
	SSW_DBG("Current sim mode(%d), GPIO0_MODE(%d, %d), GPIO1_MODE(%d, %d), GPIO2_MODE(%d, %d), GPIO3_MODE(%d, %d), GPIO89_MODE(%d, %d), GPIO90_MODE(%d, %d)\n", \
		mode, mt_get_gpio_mode(GPIO0), mt_get_gpio_dir(GPIO0), mt_get_gpio_mode(GPIO1), mt_get_gpio_dir(GPIO1), \
			  mt_get_gpio_mode(GPIO2), mt_get_gpio_dir(GPIO2), mt_get_gpio_mode(GPIO3), mt_get_gpio_dir(GPIO3), \
			  mt_get_gpio_mode(GPIO89), mt_get_gpio_dir(GPIO89), mt_get_gpio_mode(GPIO90), mt_get_gpio_dir(GPIO90));
#else
	#if defined(GPIO_SIM1_SCLK) && defined(GPIO_SIM1_SIO) && defined(GPIO_SIM2_SCLK) && defined(GPIO_SIM2_SIO)
	SSW_DBG("Current sim mode(%d), GPIO_SIM1_SCLK_MODE(%d, %d), GPIO_SIM1_SIO_MODE(%d, %d), GPIO_SIM2_SCLK_MODE(%d, %d), GPIO_SIM2_SIO_MODE(%d, %d)\n", \
		mode, mt_get_gpio_mode(GPIO_SIM1_SCLK), mt_get_gpio_dir(GPIO_SIM1_SCLK), mt_get_gpio_mode(GPIO_SIM1_SIO), mt_get_gpio_dir(GPIO_SIM1_SIO), \
			  mt_get_gpio_mode(GPIO_SIM2_SCLK), mt_get_gpio_dir(GPIO_SIM2_SCLK), mt_get_gpio_mode(GPIO_SIM2_SIO), mt_get_gpio_dir(GPIO_SIM2_SIO));
	#endif
#endif
	
	return SSW_SUCCESS;
}


int switch_sim_mode(int id, char *buf, unsigned int len)
{
	unsigned int mode = *((unsigned int *)buf);

	SSW_DBG("sim switch: %d(%d)\n", mode, sim_mode_curr);

	mutex_lock(&sim_switch_mutex);
	
	if ((sim_mode_curr != mode) && (SSW_SUCCESS == set_sim_gpio(mode)))
	{
		sim_mode_curr = mode;
	}
	
	mutex_unlock(&sim_switch_mutex);

	SSW_DBG("sim switch(%d) OK\n", sim_mode_curr);

	return 0;
	
}
EXPORT_SYMBOL(switch_sim_mode);

//To decide sim mode according to compile option
static int get_sim_mode_init(void)
{
	unsigned int sim_mode = 0;
	unsigned int md1_enable, md2_enable = 0;
	
	md1_enable = get_modem_is_enabled(MD_SYS1);
	md2_enable = get_modem_is_enabled(MD_SYS2);
	
	if (md1_enable){
		sim_mode = SINGLE_TALK_MDSYS;
		if (md2_enable)
			sim_mode = DUAL_TALK;
	}
	else if (md2_enable)
		sim_mode = SINGLE_TALK_MDSYS_LITE;
	
	return sim_mode;
}

//sim switch hardware initial
static int sim_switch_init(void) 
{
	SSW_DBG("sim_switch_init\n");
	
	//better to set pull_en and pull_sel first, then mode
	//if GPIO in sim mode, no need to set direction, because hw has done this when setting mode
	/*
	mt_set_gpio_dir(GPIO_SIM1_SCLK, GPIO_DIR_OUT); 	//GPIO0->SIM2_CLK, out
	mt_set_gpio_dir(GPIO_SIM1_SIO, GPIO_DIR_IN);  	//GPIO1->SIM2_SIO, in
	mt_set_gpio_dir(GPIO_SIM2_SCLK, GPIO_DIR_OUT); 	//GPIO2->SIM1_CLK, out
	mt_set_gpio_dir(GPIO_SIM2_SIO, GPIO_DIR_IN); 	//GPIO3->SIM1_SIO, in
	*/
	//mt_set_gpio_dir(GPIO89, GPIO_DIR_OUT);	//GPIO89->SIM1_SRST, out, 6572 not use reset pin
	//mt_set_gpio_dir(GPIO90, GPIO_DIR_OUT);	//GPIO90->SIM2_SRST, out, 6572 not use reset pin	
	
	sim_mode_curr = get_sim_mode_init();
	if (SSW_SUCCESS != set_sim_gpio(sim_mode_curr))
	{
		SSW_DBG("sim_switch_init fail \n");
		return SSW_INVALID_PARA;
	}

	return 0;
}


static int sim_switch_probe(struct platform_device *dev)
{
	SSW_DBG("Enter sim_switch_probe\n");
		
	//sim_switch_init();
	
    //move this to sim_switch_driver_init(). Because this function not exceute on device tree branch.   
	//mutex_init(&sim_switch_mutex);
	
	//register_ccci_kern_func(ID_SSW_SWITCH_MODE, switch_sim_mode);
	
	return 0;
}

static int sim_switch_remove(struct platform_device *dev)
{
	//SSW_DBG("sim_switch_remove \n");
	return 0;
}

static void sim_switch_shutdown(struct platform_device *dev)
{
	//SSW_DBG("sim_switch_shutdown \n");
}

static int sim_switch_suspend(struct platform_device *dev, pm_message_t state)
{
	//SSW_DBG("sim_switch_suspend \n");
	return 0;
}

static int sim_switch_resume(struct platform_device *dev)
{
	//SSW_DBG("sim_switch_resume \n");
	return 0;
}


static struct platform_driver sim_switch_driver =
{
	.driver     = {
		.name	= "sim-switch",
	},
	.probe		= sim_switch_probe,
	.remove		= sim_switch_remove,
	.shutdown	= sim_switch_shutdown,
	.suspend	= sim_switch_suspend,
	.resume		= sim_switch_resume,
};


static int __init sim_switch_driver_init(void)
{
	int ret = 0;

	SSW_DBG("sim_switch_driver_init\n");
	ret = platform_driver_register(&sim_switch_driver);
	if (ret) {
		SSW_DBG("ssw_driver register fail(%d)\n", ret);
		return ret;
	}

    mutex_init(&sim_switch_mutex);

#if __ENABLE_SSW_SYSFS
	ssw_sysfs_init();
#endif

	sim_switch_init();
	 
	return ret;
}
 

static void __exit sim_switch_driver_exit(void)
{
	return;
}


module_init(sim_switch_driver_init);
module_exit(sim_switch_driver_exit);


MODULE_DESCRIPTION("MTK SIM Switch Driver");
MODULE_AUTHOR("Anny <Anny.Hu@mediatek.com>");
MODULE_LICENSE("GPL");
