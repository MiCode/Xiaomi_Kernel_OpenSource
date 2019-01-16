#ifndef _MT_GPIO_H_
#define _MT_GPIO_H_

/* mark for delete, need remove other driver build error */
#include <linux/ioctl.h>
#include <linux/fs.h>

/* FIX-ME: */
/* #if (defined(CONFIG_MT6589_FPGA)) */
#if (defined(CONFIG_MTK_FPGA))
/* #define CONFIG_MT_GPIO_FPGA_ENABLE */
#include <mach/mt_gpio_fpga.h>
#else
/* FIX-ME: marked for early porting */
#include <cust_gpio_usage.h>
#include <mach/mt_gpio_base.h>
#include <mach/mt_gpio_affix.h>
#endif
#include <mach/mt_gpio_ext.h>

/******************************************************************************
* Enumeration for GPIO pin
******************************************************************************/
/* GPIO MODE CONTROL VALUE*/
typedef enum {
	GPIO_MODE_UNSUPPORTED = -1,
	GPIO_MODE_GPIO = 0,
	GPIO_MODE_00 = 0,
	GPIO_MODE_01 = 1,
	GPIO_MODE_02 = 2,
	GPIO_MODE_03 = 3,
	GPIO_MODE_04 = 4,
	GPIO_MODE_05 = 5,
	GPIO_MODE_06 = 6,
	GPIO_MODE_07 = 7,

	GPIO_MODE_MAX,
	GPIO_MODE_DEFAULT = GPIO_MODE_01,
} GPIO_MODE;
/*----------------------------------------------------------------------------*/
/* GPIO DIRECTION */
typedef enum {
	GPIO_DIR_UNSUPPORTED = -1,
	GPIO_DIR_IN = 0,
	GPIO_DIR_OUT = 1,

	GPIO_DIR_MAX,
	GPIO_DIR_DEFAULT = GPIO_DIR_IN,
} GPIO_DIR;
/*----------------------------------------------------------------------------*/
/* GPIO PULL ENABLE*/
typedef enum {
	GPIO_PULL_EN_UNSUPPORTED = -1,
	GPIO_PULL_DISABLE = 0,
	GPIO_PULL_ENABLE = 1,

	GPIO_PULL_EN_MAX,
	GPIO_PULL_EN_DEFAULT = GPIO_PULL_ENABLE,
} GPIO_PULL_EN;
/*----------------------------------------------------------------------------*/
/* GPIO SMT*/
typedef enum {
	GPIO_SMT_UNSUPPORTED = -1,
	GPIO_SMT_DISABLE = 0,
	GPIO_SMT_ENABLE  = 1,
	
	GPIO_SMT_MAX,
	GPIO_SMT_DEFAULT = GPIO_SMT_ENABLE,
} GPIO_SMT;
/*----------------------------------------------------------------------------*/
/* GPIO IES*/
typedef enum {
	GPIO_IES_UNSUPPORTED = -1,
	GPIO_IES_DISABLE = 0,
	GPIO_IES_ENABLE = 1,

	GPIO_IES_MAX,
	GPIO_IES_DEFAULT = GPIO_IES_ENABLE,
} GPIO_IES;
/*----------------------------------------------------------------------------*/
/* GPIO PULL-UP/PULL-DOWN*/
typedef enum {
	GPIO_PULL_UNSUPPORTED = -1,
	GPIO_PULL_DOWN = 0,
	GPIO_PULL_UP = 1,

	GPIO_PULL_MAX,
	GPIO_PULL_DEFAULT = GPIO_PULL_DOWN
} GPIO_PULL;
/*----------------------------------------------------------------------------*/
/* GPIO INVERSION */
typedef enum {
	GPIO_DATA_INV_UNSUPPORTED = -1,
	GPIO_DATA_UNINV = 0,
	GPIO_DATA_INV = 1,

	GPIO_DATA_INV_MAX,
	GPIO_DATA_INV_DEFAULT = GPIO_DATA_UNINV
} GPIO_INVERSION;
/*----------------------------------------------------------------------------*/
/* GPIO OUTPUT */
typedef enum {
	GPIO_OUT_UNSUPPORTED = -1,
	GPIO_OUT_ZERO = 0,
	GPIO_OUT_ONE = 1,

	GPIO_OUT_MAX,
	GPIO_OUT_DEFAULT = GPIO_OUT_ZERO,
	GPIO_DATA_OUT_DEFAULT = GPIO_OUT_ZERO,	/*compatible with DCT */
} GPIO_OUT;
/*----------------------------------------------------------------------------*/
/* GPIO INPUT */
typedef enum {
	GPIO_IN_UNSUPPORTED = -1,
	GPIO_IN_ZERO = 0,
	GPIO_IN_ONE = 1,

	GPIO_IN_MAX,
} GPIO_IN;

/******************************************************************************
* GPIO Driver interface
******************************************************************************/
/*direction*/
int mt_set_gpio_dir(unsigned long pin, unsigned long dir);
int mt_get_gpio_dir(unsigned long pin);

/*pull enable*/
int mt_set_gpio_pull_enable(unsigned long pin, unsigned long enable);
int mt_get_gpio_pull_enable(unsigned long pin);

/*schmitt trigger*/
int mt_set_gpio_smt(unsigned long pin, unsigned long enable);
int mt_get_gpio_smt(unsigned long pin);

/*IES*/
int mt_set_gpio_ies(unsigned long pin, unsigned long enable);
int mt_get_gpio_ies(unsigned long pin);

/*pull select*/
int mt_set_gpio_pull_select(unsigned long pin, unsigned long select);
int mt_get_gpio_pull_select(unsigned long pin);

/*data inversion*/
int mt_set_gpio_inversion(unsigned long pin, unsigned long enable);
int mt_get_gpio_inversion(unsigned long pin);

/*input/output*/
int mt_set_gpio_out(unsigned long pin, unsigned long output);
int mt_get_gpio_out(unsigned long pin);
int mt_get_gpio_in(unsigned long pin);

/*mode control*/
int mt_set_gpio_mode(unsigned long pin, unsigned long mode);
int mt_get_gpio_mode(unsigned long pin);

/*misc functions for protect GPIO*/
/* void mt_gpio_dump(GPIO_REGS *regs,GPIOEXT_REGS *regs_ext); */
void gpio_dump_regs(void);

#endif				/* _MT_GPIO_H_ */
