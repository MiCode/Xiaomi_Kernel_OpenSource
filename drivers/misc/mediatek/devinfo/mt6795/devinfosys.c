#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioctl.h>
#include <linux/xlog.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include "devinfo.h"

#define DEVINFO_TAG "DEVINFO"

/***************************************************************************** 
* FUNCTION DEFINITION 
*****************************************************************************/
extern u32 get_devinfo_with_index(u32 index);
extern u32 g_devinfo_data_size;

/************************************************************************** 
*  DEV DRIVER SYSFS 
**************************************************************************/ 

struct devinfo_driver {    
    struct device_driver driver;    
    const struct platform_device_id *id_table;
};


static struct devinfo_driver dev_info ={
    .driver  = {        
        .name = "dev_info",        
        .bus = &platform_bus_type,        
        .owner = THIS_MODULE,    
   },   
   .id_table = NULL,
};

static ssize_t devinfo_show(struct device_driver *driver, char *buf)
{  
	  unsigned int i;
	  unsigned int *output = (unsigned int *)buf;
	  
	  output[0] = g_devinfo_data_size;

	  for(i = 0; i < g_devinfo_data_size; i++)
	  {
        output[i+1] = get_devinfo_with_index(i);
	  }
    return (g_devinfo_data_size + 1) * sizeof(unsigned int);
}

DRIVER_ATTR(dev_info, 0444, devinfo_show, NULL);

static int __init devinfo_init(void)
{
    int ret = 0;

    /* register driver and create sysfs files */        
    ret = driver_register(&dev_info.driver);        
    if (ret)         
    {            
        printk("fail to register devinfo driver\n");        
    }        

    ret = driver_create_file(&dev_info.driver, &driver_attr_dev_info);        
    if (ret)         
    {           
        printk("[BOOT INIT] Fail to create devinfo sysfs file\n");       
    }
    return 0;
}
module_init(devinfo_init);

