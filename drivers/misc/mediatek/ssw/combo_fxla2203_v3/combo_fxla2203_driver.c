
#include <ssw.h>
#include <mach/mt_ccci_common.h>
#include "cust_gpio_usage.h"

/*--------------Feature option---------------*/
#define __ENABLE_SSW_SYSFS 1


/*--------------SIM mode list----------------*/
#define SINGLE_TALK_MDSYS                (0x1)
#define SINGLE_TALK_MDSYS_LITE           (0x2)
#define DUAL_TALK                        (0x3)
#define DUAL_TALK_SWAP                   (0x4)    

static unsigned int ext_ssw_mode_curr = DUAL_TALK;
static unsigned int sim_mode_curr = SINGLE_TALK_MDSYS;

struct mutex sim_switch_mutex;

static int set_sim_gpio(unsigned int mode);
static int set_ext_sim_gpio(unsigned int mode);

static int get_ext_current_ssw_mode(void);


unsigned int get_sim_switch_type(void)
{
    printk("[ccci/ssw]COMBO_FXLA2203_V3\n");
    return SSW_EXT_SINGLE_2X2;
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

    len = scnprintf(ptr, remain, "0x%x\n", get_ext_current_ssw_mode());
    ptr += len;
    remain -= len;
    SSW_DBG("ssw_mode_show\n");

    return (PAGE_SIZE-remain);
}
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
            if (ext_ssw_mode_curr != mode && (SSW_SUCCESS == set_ext_sim_gpio(mode))) 
            {           
                ext_ssw_mode_curr = mode;
            }
        }
    }
    return size;
}
/*---------------------------------------------------------------------------*/

/*************************************************************************/
/* external sim switch hardware operation                                                               */
/*                                                                                                                      */
/*************************************************************************/

//Exteranl sim switch hardware initial
static int extern_ssw_init(void) 
{
    unsigned int mode = DUAL_TALK;

    SSW_DBG("extern_ssw_init: %d \n", mode);
    
    //SIM_CH_SWAP
    mt_set_gpio_mode(GPIO_SSW_CH_SWAP_PIN, GPIO_SSW_CH_SWAP_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_SSW_CH_SWAP_PIN, GPIO_DIR_OUT);
    //SIM_ENABLE
    mt_set_gpio_mode(GPIO_SSW_EN_PIN, GPIO_SSW_EN_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_SSW_EN_PIN, GPIO_DIR_OUT); 
    mt_set_gpio_out(GPIO_SSW_EN_PIN, GPIO_OUT_ONE);

    SSW_DBG("extern_ssw_init: ch_swap=(%x %d %d), en=(%x %d %d) \n", 
        GPIO_SSW_CH_SWAP_PIN, GPIO_SSW_CH_SWAP_PIN_M_GPIO, mt_get_gpio_out(GPIO_SSW_CH_SWAP_PIN),
        GPIO_SSW_EN_PIN, GPIO_SSW_EN_PIN_M_GPIO, mt_get_gpio_out(GPIO_SSW_EN_PIN));
    set_ext_sim_gpio(mode);
    ext_ssw_mode_curr = mode;
    return 0;
}

static int set_sim_gpio(unsigned int mode)
{
    SSW_DBG("set_sim_gpio %d\n", mode);
    
    switch(mode)
    {
        case SINGLE_TALK_MDSYS:
            set_ext_sim_gpio(SINGLE_TALK_MDSYS);
            break;
        
        default:
            SSW_DBG("Mode(%d) not supported!!!", mode);
            break;
    }
    
    return SSW_SUCCESS;
}
static int set_ext_sim_gpio(unsigned int mode)
{
    SSW_DBG("set_ext_sim_gpio %d\n", mode);
    
    switch(mode)
    {
        case DUAL_TALK:
                 mt_set_gpio_out(GPIO_SSW_CH_SWAP_PIN, GPIO_OUT_ZERO);
         break;
        case DUAL_TALK_SWAP:
                 mt_set_gpio_out(GPIO_SSW_CH_SWAP_PIN, GPIO_OUT_ONE);
                 break;
        default:
            SSW_DBG("Mode(%d) not supported!!!", mode);
            return SSW_INVALID_PARA;
    }

    SSW_DBG("set_ext_sim_gpio: ch_swap=(%x %d %d), en=(%x %d %d) \n", 
        GPIO_SSW_CH_SWAP_PIN, GPIO_SSW_CH_SWAP_PIN_M_GPIO, mt_get_gpio_out(GPIO_SSW_CH_SWAP_PIN),
        GPIO_SSW_EN_PIN, GPIO_SSW_EN_PIN_M_GPIO, mt_get_gpio_out(GPIO_SSW_EN_PIN));
    
    return SSW_SUCCESS;
}

int get_ext_current_ssw_mode(void)
{
    return ext_ssw_mode_curr;
}

int switch_sim_mode(int id, char *buf, unsigned int len)
{
    unsigned int mode = *((unsigned int *)buf);
    unsigned int type = (mode&0xFFFF0000)>>16;
    SSW_DBG("switch_sim_mode:mode=0x%x, type=%d\n", mode, type);

    mode = mode&0x0000FFFF;

    mutex_lock(&sim_switch_mutex);
    if(type == 0) { // Internal
        SSW_DBG("Internal sim switch: %d --> %d\n", sim_mode_curr, mode);
        if ((sim_mode_curr != mode) && (SSW_SUCCESS == set_sim_gpio(mode)))
        {
            sim_mode_curr = mode;
        }
    } else { //External 
        SSW_DBG("External sim switch: %d --> %d\n", ext_ssw_mode_curr, mode);
        if (ext_ssw_mode_curr != mode && (SSW_SUCCESS == set_ext_sim_gpio(mode))) 
        {            
            ext_ssw_mode_curr = mode;
        }
    }    
    mutex_unlock(&sim_switch_mutex);

    SSW_DBG("sim switch sim_mode_curr(%d),ext_ssw_mode_curr(%d) OK\n", sim_mode_curr,ext_ssw_mode_curr);

    return 0;
    
}
EXPORT_SYMBOL(switch_sim_mode);

//To decide sim mode according to compile option
static int get_sim_mode_init(void)
{
    unsigned int sim_mode = 0;
#if 0   
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
#endif
    return sim_mode;
}

//sim switch hardware initial
static int sim_switch_init(void) 
{
    SSW_DBG("sim_switch_init\n");
    
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
    //move this to sim_switch_driver_init(). Because this function not exceute on device tree branch.   
    //mutex_init(&sim_switch_mutex);
    sim_switch_init();
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
        .name    = "sim-switch",
    },
    .probe        = sim_switch_probe,
    .remove        = sim_switch_remove,
    .shutdown    = sim_switch_shutdown,
    .suspend    = sim_switch_suspend,
    .resume        = sim_switch_resume,
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
MODULE_AUTHOR("MTK");
MODULE_LICENSE("GPL");
