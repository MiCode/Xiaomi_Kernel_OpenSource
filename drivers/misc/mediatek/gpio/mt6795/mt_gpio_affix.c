/******************************************************************************
 * mt_gpio_affix.c - MTKLinux GPIO Device Driver
 * 
 * Copyright 2008-2009 MediaTek Co.,Ltd.
 * 
 * DESCRIPTION:
 *     This file provid the other drivers GPIO relative functions
 *
 ******************************************************************************/

#include <linux/types.h>
#include <linux/aee.h>

#include <mach/sync_write.h>
#include <mach/mt_typedefs.h>
#include <linux/device.h>
//#include <mach/mt_gpio.h>
#include <mach/mt_gpio_core.h>
#include <mach/mt_gpio_affix.h>

#define GPIO_WR32(addr, data)   mt65xx_reg_sync_writel(data, addr)
#define GPIO_RD32(addr)         __raw_readl(addr)

void mt_gpio_pin_decrypt(unsigned long *cipher)
{
	//just for debug, find out who used pin number directly
	if((*cipher & (0x80000000)) == 0){
		GPIOERR("GPIO%u HARDCODE warning!!! \n",(unsigned int)(*cipher));	
		aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, "GPIO", "GPIO hard code");
		//dump_stack();
		//return;
	}

	//GPIOERR("Pin magic number is %x\n",*cipher);
	*cipher &= ~(0x80000000);
	return;
}

extern ssize_t mt_gpio_show_pin(struct device* dev, struct device_attribute *attr, char *buf);
extern ssize_t mt_gpio_store_pin(struct device* dev, struct device_attribute *attr, const char *buf, size_t count);

/*---------------------------------------------------------------------------*/
static DEVICE_ATTR(pin,      0664, mt_gpio_show_pin,   mt_gpio_store_pin);
/*---------------------------------------------------------------------------*/
static struct device_attribute *gpio_attr_list[] = {
    &dev_attr_pin,
};

/*---------------------------------------------------------------------------*/
int mt_gpio_create_attr(struct device *dev) 
{
    int idx, err = 0;
    int num = (int)(sizeof(gpio_attr_list)/sizeof(gpio_attr_list[0]));
    if (!dev)
        return -EINVAL;

    for (idx = 0; idx < num; idx++) {
        if ((err = device_create_file(dev, gpio_attr_list[idx])))
            break;
    }
    
    return err;
}
/*---------------------------------------------------------------------------*/
int mt_gpio_delete_attr(struct device *dev)
{
    int idx ,err = 0;
    int num = (int)(sizeof(gpio_attr_list)/sizeof(gpio_attr_list[0]));
    
    if (!dev)
        return -EINVAL;

    for (idx = 0; idx < num; idx++) 
        device_remove_file(dev, gpio_attr_list[idx]);

    return err;
}

