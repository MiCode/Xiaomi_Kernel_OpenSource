#ifndef __SSW_H__
#define __SSW_H__
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/delay.h>

#include <linux/kdev_t.h>
#include <linux/ctype.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <mach/mt_typedefs.h>
//#include <mach/mtk_ccci_helper.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_gpio.h>


/*-------------------------debug log define--------------------------------*/
static int dbg_en = 1;
#define SSW_DBG(format, args...) do{ \
	if(dbg_en) \
	{\
		printk(KERN_ERR "[ccci\\ssw] "format,##args);\
	}\
}while(0)


/*-------------------------variable define----------------------------------*/
#if 0
#ifndef SSW_DUAL_TALK
#define SSW_DUAL_TALK 0
#endif

#ifndef SSW_SING_TALK
#define SSW_SING_TALK 1
#endif
#endif

/*------------------------Error Code---------------------------------------*/
#define SSW_SUCCESS 			(0)
#define SSW_INVALID_PARA		(-1)

enum {
	SSW_INVALID = 0xFFFFFFFF,
	SSW_INTERN = 0,
	SSW_EXT_FXLA2203 = 1,
	SSW_EXT_SINGLE_COMMON = 2,
	SSW_EXT_DUAL_1X2 = 3,
	SSW_EXT_SINGLE_2X2 = 4,
	
	SSW_RESTORE = 0x5AA5,
};

#endif


