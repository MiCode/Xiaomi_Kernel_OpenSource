
#include "cust_ssw.h"
#include <ssw.h>
#include <mach/mtk_ccci_helper.h>
#include "cust_gpio_usage.h"
#ifdef MTK_PCA9575A_SUPPORT
#include "cust_pca9575.h"
#endif

/*--------------Feature option---------------*/
#define __ENABLE_SSW_SYSFS 1


/*--------------SIM mode list----------------*/
#define SINGLE_TALK_MDSYS				(0x1)
#define SINGLE_TALK_MDSYS_LITE			(0x2)
#define DUAL_TALK						(0x3)
#define DUAL_TALK_SWAP					(0x4)	

static unsigned int ext_ssw_mode_curr = SSW_DUAL_TALK;
static unsigned int ch_swap;
static unsigned int en;
static unsigned int sim_mode_curr = SINGLE_TALK_MDSYS;

struct mutex sim_switch_mutex;

static int sim_switch_init(void);

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
static int get_ext_current_ssw_mode(void);


unsigned int get_sim_switch_type(void)
{
	printk("[ccci/ssw]COMBO_FXLA2203\n");
	return SSW_EXT_FXLA2203;
}
EXPORT_SYMBOL(get_sim_switch_type);

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

	len = scnprintf(ptr, remain, "I:0x%x E:0x%x\n", get_current_ssw_mode(), get_ext_current_ssw_mode());
	ptr += len;
	remain -= len;
	SSW_DBG("ssw_mode_show\n");

	return (PAGE_SIZE-remain);
}
#ifdef MTK_PCA9575A_SUPPORT
ssize_t ssw_mode_store(struct kobject *kobj, const char *buffer, size_t size) 
{
	unsigned int mode;
	int res = sscanf(buffer, "%x", &mode);
	unsigned int type;

	if (res != 1)
	{
		printk("%s: expect 1 numbers\n", __FUNCTION__);
	}
	else
	{
		SSW_DBG("ssw_mode_store %x\n", mode);
		//Switch sim mode
		type = (mode&0xFFFF0000)>>16;
		mode = mode&0x0000FFFF;
		if(type == 0) { // Internal
			SSW_DBG("Internal sim switch: %d-->%d\n", sim_mode_curr, mode);
			if ((sim_mode_curr != mode) && (SSW_SUCCESS == set_sim_gpio(mode)))
			{
				sim_mode_curr = mode;
			}
		} else {
			SSW_DBG("External sim switch: %d-->%d\n", ext_ssw_mode_curr, mode);
			if (mode == SSW_DUAL_TALK) {
				//mt_set_gpio_out(ch_swap, SSW_DUAL_TALK);
				pca9575_set_gpio_output(ch_swap, SSW_DUAL_TALK);
		
			} else if (mode == SSW_SING_TALK) {
				pca9575_set_gpio_output(ch_swap, SSW_SING_TALK);
				//mt_set_gpio_out(ch_swap, SSW_SING_TALK);
			}
			ext_ssw_mode_curr = mode;
			//mt_set_gpio_out(en, GPIO_OUT_ONE);
			pca9575_set_gpio_output(en, GPIO_OUT_ONE);
		}
	}
	return size;
}
#else
ssize_t ssw_mode_store(struct kobject *kobj, const char *buffer, size_t size) 
{
	unsigned int mode;
	int res = sscanf(buffer, "%x", &mode);
	unsigned int type;
	if (res != 1)
	{
		printk("%s: expect 1 numbers\n", __FUNCTION__);
	}
	else
	{
		SSW_DBG("ssw_mode_store %x\n", mode);
		type = (mode&0xFFFF0000)>>16;
		mode = mode&0x0000FFFF;
		if(type == 0) { // Internal
			SSW_DBG("Internal sim switch: %d-->%d\n", sim_mode_curr, mode);
			if ((sim_mode_curr != mode) && (SSW_SUCCESS == set_sim_gpio(mode)))
			{
				sim_mode_curr = mode;
			}
		} else {
			SSW_DBG("External sim switch: %d-->%d\n", ext_ssw_mode_curr, mode);
			if (mode == SSW_DUAL_TALK) {
				mt_set_gpio_out(ch_swap, SSW_DUAL_TALK);
			} else if (mode == SSW_SING_TALK) {
				mt_set_gpio_out(ch_swap, SSW_SING_TALK);
			}
			ext_ssw_mode_curr = mode;
			mt_set_gpio_out(en, GPIO_OUT_ONE);
		}
	}
	return size;
}
#endif
/*---------------------------------------------------------------------------*/

/*************************************************************************/
/* external sim switch hardware operation                                                               */
/*                                                                                                                      */
/*************************************************************************/

//Exteranl sim switch hardware initial
#ifdef MTK_PCA9575A_SUPPORT
static int extern_ssw_init(void) 
{
	unsigned int mode = SSW_DUAL_TALK;
	unsigned int ch_mode, en_mode;

	SSW_DBG("extern_ssw_init: %d \n", mode);
	
	//ch_swap = GPIO_SSW_CH_SWAP_PIN;
	//en = GPIO_SSW_EN_PIN;
	en = GPIO_SSW_ENABLE;//GPIO_P0_1;
	ch_swap = GPIO_SSW_CH_SW;//GPIO_P0_2;
	ch_mode = GPIO_SSW_CH_SWAP_PIN_M_GPIO;
	en_mode = GPIO_SSW_EN_PIN_M_GPIO;

	
	//initial Ch_Swap pin: 1, host1->sim slot1, host2->sim slot2; 0, host1->sim slot2, host2->sim slot1
	//mt_set_gpio_mode(ch_swap, ch_mode);
	//mt_set_gpio_dir(ch_swap, GPIO_DIR_OUT);
	pca9575_set_gpio_dir(ch_swap,0); //set as output

	//initial EN pin: 1, enable sim slot; 0, disable sim slot
	//mt_set_gpio_mode(en, en_mode);
	//mt_set_gpio_dir(en, GPIO_DIR_OUT);
	pca9575_set_gpio_dir(en,0); //set as output

	ext_ssw_mode_curr = mode;
	if (mode == SSW_DUAL_TALK) {
		//mt_set_gpio_out(ch_swap, SSW_DUAL_TALK);
		pca9575_set_gpio_output(ch_swap, SSW_DUAL_TALK);
		
	} else if (mode == SSW_SING_TALK) {
		//mt_set_gpio_out(ch_swap, SSW_SING_TALK);
		pca9575_set_gpio_output(ch_swap, SSW_SING_TALK);
	}

	mdelay(50); //delay 50ms
	//mt_set_gpio_out(en, GPIO_OUT_ONE);
	pca9575_set_gpio_output(en, GPIO_OUT_ONE);

	SSW_DBG("extern_ssw_init: ch_swap=(%x %d %d), en=(%x %d %d) \n", 
			ch_swap, ch_mode, pca9575_get_gpio_output(ch_swap),
			en, en_mode, pca9575_get_gpio_output(en));

	return 0;
}
#else
static int extern_ssw_init(void) 
{
	unsigned int mode = SSW_DUAL_TALK;
	unsigned int ch_mode, en_mode;
	SSW_DBG("extern_ssw_init: %d \n", mode);
	ch_swap = GPIO_SSW_CH_SWAP_PIN;
	en = GPIO_SSW_EN_PIN;
	ch_mode = GPIO_SSW_CH_SWAP_PIN_M_GPIO;
	en_mode = GPIO_SSW_EN_PIN_M_GPIO;
	mt_set_gpio_mode(ch_swap, ch_mode);
	mt_set_gpio_dir(ch_swap, GPIO_DIR_OUT);
	mt_set_gpio_mode(en, en_mode);
	mt_set_gpio_dir(en, GPIO_DIR_OUT);
	ext_ssw_mode_curr = mode;
	if (mode == SSW_DUAL_TALK) {
		mt_set_gpio_out(ch_swap, SSW_DUAL_TALK);
	} else if (mode == SSW_SING_TALK) {
		mt_set_gpio_out(ch_swap, SSW_SING_TALK);
	}
	mt_set_gpio_out(en, GPIO_OUT_ONE);
	SSW_DBG("extern_ssw_init: ch_swap=(%x %d %d), en=(%x %d %d) \n", 
		ch_swap, ch_mode, mt_get_gpio_out(ch_swap),
		en, en_mode, mt_get_gpio_out(en));
	return 0;
}
#endif

static int set_sim_gpio(unsigned int mode)
{
	SSW_DBG("set_sim_gpio %d\n", mode);
	
	switch(mode)
	{
		case SINGLE_TALK_MDSYS:
			mt_set_gpio_mode(GPIO_SIM1_SCLK, 1);	//SIM1_SCLK	
			mt_set_gpio_mode(GPIO_SIM1_SIO, 1); 	//SIM1_SIO
			mt_set_gpio_mode(GPIO_SIM2_SCLK, 1); 	//SIM2_SCLK
			mt_set_gpio_mode(GPIO_SIM2_SIO, 1); 	//SIM2_SIO
			//mt_set_gpio_mode(GPIO_SIM1_SRST, 4);	//SIM1_SRST, 6582 not use reset pin
			//mt_set_gpio_mode(GPIO_SIM2_SRST, 4);	//SIM2_SRST, 6582 not use reset pin
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
	SSW_DBG("Current sim mode(%d), GPIO_SIM1_SCLK_MODE(%d, %d), GPIO_SIM1_SIO_MODE(%d, %d), GPIO_SIM2_SCLK_MODE(%d, %d), GPIO_SIM2_SIO_MODE(%d, %d)\n", \
		mode, mt_get_gpio_mode(GPIO_SIM1_SCLK), mt_get_gpio_dir(GPIO_SIM1_SCLK), mt_get_gpio_mode(GPIO_SIM1_SIO), mt_get_gpio_dir(GPIO_SIM1_SIO), \
			  mt_get_gpio_mode(GPIO_SIM2_SCLK), mt_get_gpio_dir(GPIO_SIM2_SCLK), mt_get_gpio_mode(GPIO_SIM2_SIO), mt_get_gpio_dir(GPIO_SIM2_SIO));
#endif
	
	return SSW_SUCCESS;
}

int get_ext_current_ssw_mode(void)
{
	return ext_ssw_mode_curr;
}

int get_current_ssw_mode(void)
{
	return sim_mode_curr;
}

#ifdef MTK_PCA9575A_SUPPORT
int switch_sim_mode(int id, char *buf, unsigned int len)
{
	unsigned int mode = *((unsigned int *)buf);
	unsigned int type = (mode&0xFFFF0000)>>16;
	int			 direction = 0;

	mode = mode&0x0000FFFF;

	mutex_lock(&sim_switch_mutex);
	
	if (type == SSW_RESTORE) {
		// This used for IPO-H to restore sim to default state
		SSW_DBG("sim switch restore\n");
		sim_switch_init(); 
	} else if(type == 0) { // Internal
		SSW_DBG("Internal sim switch: %d --> %d\n", sim_mode_curr, mode);
		if ((sim_mode_curr != mode) && (SSW_SUCCESS == set_sim_gpio(mode)))
		{
			sim_mode_curr = mode;
		}
	} else if (type == SSW_EXT_SINGLE_COMMON){ //External
		if( ((mode&0x00FF) == 0) || ((mode&0xFF00) == 0) ) {// CDMA
			direction = SSW_DUAL_TALK;
		} else {
			direction = SSW_SING_TALK;
		}
		SSW_DBG("External sim switch: %d --> %d\n", ext_ssw_mode_curr, direction);
		if (ext_ssw_mode_curr != direction) {
			if (direction == SSW_DUAL_TALK)
				//mt_set_gpio_out(ch_swap, SSW_DUAL_TALK);
				pca9575_set_gpio_output(ch_swap, SSW_DUAL_TALK);
			else if (direction == SSW_SING_TALK)
				pca9575_set_gpio_output(ch_swap, SSW_SING_TALK);
				//mt_set_gpio_out(ch_swap, SSW_SING_TALK);	
	
			//SSW_DBG("ch_swap=%d, en=%d \n", mt_get_gpio_out(ch_swap), mt_get_gpio_out(en));
			SSW_DBG("ch_swap=%d, en=%d \n", pca9575_get_gpio_output(ch_swap), pca9575_get_gpio_output(en));
			ext_ssw_mode_curr = direction;
		}
	} else { //External 
		SSW_DBG("External sim switch: %d --> %d\n", ext_ssw_mode_curr, mode);
		if (ext_ssw_mode_curr != mode) {
			if (mode == SSW_DUAL_TALK)
				//mt_set_gpio_out(ch_swap, SSW_DUAL_TALK);	
				pca9575_set_gpio_output(ch_swap, SSW_DUAL_TALK);
			else if (mode == SSW_SING_TALK)
				pca9575_set_gpio_output(ch_swap, SSW_SING_TALK);
				//mt_set_gpio_out(ch_swap, SSW_SING_TALK);	
	
			//SSW_DBG("ch_swap=%d, en=%d \n", mt_get_gpio_out(ch_swap), mt_get_gpio_out(en));
			SSW_DBG("ch_swap=%d, en=%d \n", pca9575_get_gpio_output(ch_swap), pca9575_get_gpio_output(en));
			ext_ssw_mode_curr = mode;
		}
	}
	
	mutex_unlock(&sim_switch_mutex);

	SSW_DBG("sim switch(%d) OK\n", sim_mode_curr);

	return 0;
	
}
#else
int switch_sim_mode(int id, char *buf, unsigned int len)
{
	unsigned int mode = *((unsigned int *)buf);
	unsigned int type = (mode&0xFFFF0000)>>16;
	int			 direction = 0;
	mode = mode&0x0000FFFF;
	mutex_lock(&sim_switch_mutex);
	if(type == 0) { // Internal
		SSW_DBG("Internal sim switch: %d --> %d\n", sim_mode_curr, mode);
		if ((sim_mode_curr != mode) && (SSW_SUCCESS == set_sim_gpio(mode)))
		{
			sim_mode_curr = mode;
		}
	} else if (type == SSW_EXT_SINGLE_COMMON){ //External
		if( ((mode&0x00FF) == 0) || ((mode&0xFF00) == 0) ) {// CDMA
			direction = SSW_DUAL_TALK;
		} else {
			direction = SSW_SING_TALK;
		}
		SSW_DBG("External sim switch: %d --> %d\n", ext_ssw_mode_curr, direction);
		if (ext_ssw_mode_curr != direction) {
			if (direction == SSW_DUAL_TALK)
				mt_set_gpio_out(ch_swap, SSW_DUAL_TALK);		
			else if (direction == SSW_SING_TALK)
				mt_set_gpio_out(ch_swap, SSW_SING_TALK);	
			SSW_DBG("ch_swap=%d, en=%d \n", mt_get_gpio_out(ch_swap), mt_get_gpio_out(en));
			ext_ssw_mode_curr = direction;
		}
	} else { //External 
		SSW_DBG("External sim switch: %d --> %d\n", ext_ssw_mode_curr, mode);
		if (ext_ssw_mode_curr != mode) {
			if (mode == SSW_DUAL_TALK)
				mt_set_gpio_out(ch_swap, SSW_DUAL_TALK);		
			else if (mode == SSW_SING_TALK)
				mt_set_gpio_out(ch_swap, SSW_SING_TALK);	
			SSW_DBG("ch_swap=%d, en=%d \n", mt_get_gpio_out(ch_swap), mt_get_gpio_out(en));
			ext_ssw_mode_curr = mode;
		}
	}
	mutex_unlock(&sim_switch_mutex);
	SSW_DBG("sim switch(%d) OK\n", sim_mode_curr);
	return 0;
}
#endif
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
int sim_switch_init(void) 
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
	
	extern_ssw_init();

	return 0;
}


static int sim_switch_probe(struct platform_device *dev)
{
	SSW_DBG("Enter sim_switch_probe\n");
		
	//sim_switch_init();
	
	//mutex_init(&sim_switch_mutex);
	
	register_ccci_kern_func(ID_SSW_SWITCH_MODE, switch_sim_mode);
	
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


//module_init(sim_switch_driver_init);
late_initcall(sim_switch_driver_init);
module_exit(sim_switch_driver_exit);


MODULE_DESCRIPTION("MTK SIM Switch Driver");
MODULE_AUTHOR("Anny <Anny.Hu@mediatek.com>");
MODULE_LICENSE("GPL");
