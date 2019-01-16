/******************************************************************************
 * mt_gpio_fpga.c - MTKLinux GPIO Device Driver
 * 
 * Copyright 2008-2009 MediaTek Co.,Ltd.
 * 
 * DESCRIPTION:
 *     This file provid the other drivers GPIO debug functions
 *
 ******************************************************************************/
 
#include <mach/mt_gpio.h>
#include <mach/mt_gpio_core.h>
#include <mach/mt_gpio_fpga.h>

#if 0
static void mt_gpio_set_bit(unsigned long nr, unsigned long reg)
{
	unsigned long value;
	value = readl(reg);
	value |= 1L << nr;
	mt_reg_sync_writel(value, reg);
}

static void mt_gpio_clear_bit(unsigned long nr, unsigned long reg)
{
	unsigned long value;
	value = readl(reg);
	value &= ~(1L << nr);
	mt_reg_sync_writel(value, reg);
}

static unsigned long mt_gpio_get_bit(unsigned long nr , unsigned long reg)
{
	unsigned long value;
	value = readl(reg);
	value &= (1L << nr);
	return value ? 1 : 0 ;
}
#endif

int mt_set_gpio_dir_base(unsigned long pin, unsigned long dir)
{
	int ret = RSUCCESS;
#if 0	
	if (dir == GPIO_DIR_IN || dir == GPIO_DIR_DEFAULT)
		mt_gpio_clear_bit(pin, MT_GPIO_DIR_REG);
	else if (dir == GPIO_DIR_OUT)
		mt_gpio_set_bit(pin, MT_GPIO_DIR_REG);
	else
		ret = GPIO_DIR_UNSUPPORTED;
#endif
	return ret;
}
int mt_get_gpio_dir_base(unsigned long pin)
{
#if 0
	int value;
	
	value = mt_gpio_get_bit(pin, MT_GPIO_DIR_REG);
	return value;
#endif
	return GPIO_DIR_UNSUPPORTED;
}  
int mt_set_gpio_pull_enable_base(unsigned long pin, unsigned long enable)    {return RSUCCESS;}
int mt_get_gpio_pull_enable_base(unsigned long pin)                {return GPIO_PULL_EN_UNSUPPORTED;}
int mt_set_gpio_pull_select_base(unsigned long pin, unsigned long select)    {return RSUCCESS;}
int mt_get_gpio_pull_select_base(unsigned long pin)                {return GPIO_PULL_UNSUPPORTED;}
int mt_set_gpio_inversion_base(unsigned long pin, unsigned long enable)      {return RSUCCESS;}
int mt_get_gpio_inversion_base(unsigned long pin)                  {return GPIO_DATA_INV_UNSUPPORTED;}
int mt_set_gpio_smt_base(unsigned long pin, unsigned long enable)	 {return RSUCCESS;}
int mt_get_gpio_smt_base(unsigned long pin)                  {return RSUCCESS;}
int mt_set_gpio_ies_base(unsigned long pin, unsigned long enable)	 {return RSUCCESS;}
int mt_get_gpio_ies_base(unsigned long pin)                  {return GPIO_IES_UNSUPPORTED;}

int mt_set_gpio_out_base(unsigned long pin, unsigned long output)
{
	int ret = RSUCCESS;
#if 0	
	if (output == GPIO_OUT_ZERO || output == GPIO_OUT_DEFAULT || output == GPIO_DATA_OUT_DEFAULT) {
		mt_gpio_clear_bit(pin, MT_GPIO_OUT_REG);
	} else if (output == GPIO_OUT_ONE) {
		mt_gpio_set_bit(pin, MT_GPIO_OUT_REG);
	} else {
		ret = GPIO_OUT_UNSUPPORTED;
	}
#endif
	return ret;
}
int mt_get_gpio_out_base(unsigned long pin)
{
#if 0
	int value;
	
	value = mt_gpio_get_bit(pin, MT_GPIO_OUT_REG);
	return value;
#endif
	return GPIO_OUT_UNSUPPORTED;
}
int mt_get_gpio_in_base(unsigned long pin) 
{
#if 0
	int value;
	
	value = mt_gpio_get_bit(pin, MT_GPIO_IN_REG);
	return value;
#endif
	return GPIO_IN_UNSUPPORTED;
}
int mt_set_gpio_mode_base(unsigned long pin, unsigned long mode)             {return RSUCCESS;}
int mt_get_gpio_mode_base(unsigned long pin)                       {return GPIO_MODE_UNSUPPORTED;}
int mt_set_clock_output_base(unsigned long num, unsigned long src, unsigned long div)    {return RSUCCESS;}
int mt_get_clock_output_base(unsigned long num, unsigned long *src, unsigned long *div)  {return -1;}

/*---------------------------------------------------------------------------*/
void get_gpio_vbase(struct device_node *node)
{
	/* compatible with HAL */
}
/*---------------------------------------------------------------------------*/
void get_io_cfg_vbase(void)
{
	/* compatible with HAL */
}
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_MD32_SUPPORT
/*---------------------------------------------------------------------------*/
void md32_gpio_handle_init(void)
{
	/* compatible with HAL */
}
/*---------------------------------------------------------------------------*/
#endif /*CONFIG_MD32_SUPPORT*/
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM 
/*---------------------------------------------------------------------------*/
void mt_gpio_suspend(void)
{
    /* compatible with HAL */
}
/*---------------------------------------------------------------------------*/
void mt_gpio_resume(void)
{
	/* compatible with HAL */
}
/*---------------------------------------------------------------------------*/
#endif /*CONFIG_PM*/
/*---------------------------------------------------------------------------*/

#if 0
static ssize_t mt_gpio_dump_regs(char *buf, ssize_t bufLen)
{
    int idx = 0, len = 0;
	
	GPIOMSG("PIN: [DIR] [DOUT] [DIN]\n");
    for (idx = 0; idx < 8; idx++) {        
        len += snprintf(buf+len, bufLen-len, "%d: %d %d %d\n",
               idx, mt_get_gpio_dir(idx), mt_get_gpio_out(idx),mt_get_gpio_in(idx)); 
    }
	GPIOMSG("PIN: [MODE] [PULL_SEL] [DIN] [DOUT] [PULL EN] [DIR] [INV]\n");
    for (idx = GPIO_EXTEND_START; idx < MAX_GPIO_PIN; idx++) {        
		len += snprintf(buf+len, bufLen-len, "%d: %d %d %d %d %d %d %d\n",
		   idx,mt_get_gpio_mode(idx), mt_get_gpio_pull_select(idx), mt_get_gpio_in(idx),mt_get_gpio_out(idx),
		   mt_get_gpio_pull_enable(idx),mt_get_gpio_dir(idx),mt_get_gpio_inversion(idx)); 
    }

    return len;
}
#endif
/*---------------------------------------------------------------------------*/

