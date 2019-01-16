#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include <mach/eint.h>

#include <cust_eint.h>
#include <cust_gpio_usage.h>
#include <mach/mt_gpio.h>

#include "sw_tx_power.h"

#include "../tc1_interface/gpt/lg_partition.h"


#define EINT_PIN_PLUG_IN        (1)
#define EINT_PIN_PLUG_OUT       (0)

/* please turn TEST_MODE on for the test swtp feature with headset detecting */
#define TEST_MODE // by accdet

#ifdef TEST_MODE
    #if defined(CUST_EINT_ACCDET_NUM) && defined(GPIO_ACCDET_EINT_PIN)
    #define ENABLE_EINT_SWTP
    #endif
#else /* TEST_MODE */
    #if defined(CUST_EINT_SWTP_NUM) && defined(GPIO_SWTP_EINT_PIN)
    #define ENABLE_EINT_SWTP
    #endif
#endif /* TEST_MODE */

#if defined(ENABLE_EINT_SWTP)
static int swtp_eint_state = 0;

#ifdef TEST_MODE
#define SWTP_EINT_NUM		CUST_EINT_ACCDET_NUM
#define SWTP_EINT_TYPE		CUST_EINT_ACCDET_TYPE
#define SWTP_DEBOUNCE_CN	CUST_EINT_ACCDET_DEBOUNCE_CN

#define SWTP_GPIO_EINT_PIN	GPIO_ACCDET_EINT_PIN
#define SWTP_GPIO_EINT_MODE	GPIO_ACCDET_EINT_PIN_M_EINT
#else /* TEST_MODE */
#define SWTP_EINT_NUM		CUST_EINT_SWTP_NUM
#define SWTP_EINT_TYPE		CUST_EINT_SWTP_TYPE
#define SWTP_DEBOUNCE_CN	CUST_EINT_SWTP_DEBOUNCE_CN

#define SWTP_GPIO_EINT_PIN	GPIO_SWTP_EINT_PIN
#define SWTP_GPIO_EINT_MODE	GPIO_SWTP_EINT_PIN_M_EINT
#endif /* TEST_MODE */

#define SWTP_GPIO_EINT_DIR	GPIO_DIR_IN
#define SWTP_GPIO_EINT_PULL	GPIO_PULL_DISABLE
#define SWTP_SENSITIVE_TYPE	MT_LEVEL_SENSITIVE
#define SWTP_OPPOSE_EINT_TYPE	(SWTP_EINT_TYPE==CUST_EINTF_TRIGGER_HIGH)?CUST_EINTF_TRIGGER_LOW:CUST_EINTF_TRIGGER_HIGH // CUST_EINTF_TRIGGER_LOW

#define SWTP_TRIGGERING ((SWTP_EINT_TYPE == CUST_EINTF_TRIGGER_HIGH)?1:0)

extern int swtp_set_mode_unlocked(unsigned int ctrid, unsigned int enable);

static void swtp_eint_handler(void)
{
    unsigned int rfcable_enable;

    mt_eint_mask(SWTP_EINT_NUM);

    if(swtp_eint_state ==  EINT_PIN_PLUG_IN )
    {
	if (SWTP_EINT_TYPE == CUST_EINTF_TRIGGER_HIGH){
		mt_eint_set_polarity(SWTP_EINT_NUM, (1));
	}else{
		mt_eint_set_polarity(SWTP_EINT_NUM, (0));
	}
	swtp_eint_state = EINT_PIN_PLUG_OUT;
	rfcable_enable  = SWTP_MODE_OFF;
    } 
    else 
    {
	if (SWTP_EINT_TYPE == CUST_EINTF_TRIGGER_HIGH){
		mt_eint_set_polarity(SWTP_EINT_NUM, !(1));
	}else{
		mt_eint_set_polarity(SWTP_EINT_NUM, !(0));
	}
	swtp_eint_state = EINT_PIN_PLUG_IN;
        rfcable_enable  = SWTP_MODE_ON;
    }

    printk("[swtp]: rfcable_enable: %d\n", rfcable_enable);

    swtp_set_mode_unlocked(SWTP_CTRL_SUPER_SET,  rfcable_enable);
    mt_eint_unmask(SWTP_EINT_NUM);
}

extern int swtp_reset_tx_power(void);
extern int swtp_rfcable_tx_power(void);

int swtp_mod_eint_read(void)
{
    int status;

    if(mt_get_gpio_in(SWTP_GPIO_EINT_PIN) == SWTP_TRIGGERING) {
        status = swtp_rfcable_tx_power();
        printk("[swtp] cable in! [%d]\n", status);
    }
    else {
        status = swtp_reset_tx_power();
        printk("[swtp] cable out! [%d]\n", status);
    }

    return status;
}

int swtp_mod_eint_enable(void)
{
    mt_eint_unmask(SWTP_EINT_NUM);  

    return 0;
}

int swtp_mod_eint_init(void)
{
    int init_value = 0;
    unsigned int init_flag = CUST_EINTF_TRIGGER_HIGH;

    mt_set_gpio_mode(SWTP_GPIO_EINT_PIN, SWTP_GPIO_EINT_MODE);
    mt_set_gpio_dir(SWTP_GPIO_EINT_PIN, SWTP_GPIO_EINT_DIR);
    mt_set_gpio_pull_enable(SWTP_GPIO_EINT_PIN, SWTP_GPIO_EINT_PULL);

    init_value = mt_get_gpio_in(SWTP_GPIO_EINT_PIN);
    if(init_value == SWTP_TRIGGERING) {
        init_flag = SWTP_OPPOSE_EINT_TYPE; //CUST_EINTF_TRIGGER_LOW;
        swtp_eint_state = EINT_PIN_PLUG_IN;
    }
    else {
        init_flag = SWTP_EINT_TYPE; //CUST_EINTF_TRIGGER_HIGH;
        swtp_eint_state = EINT_PIN_PLUG_OUT;
    }

    mt_eint_set_sens(SWTP_EINT_NUM, SWTP_SENSITIVE_TYPE);
    mt_eint_set_hw_debounce(SWTP_EINT_NUM, SWTP_DEBOUNCE_CN);
    mt_eint_registration(SWTP_EINT_NUM, init_flag, swtp_eint_handler, 0);

    return 0;
}
#else
int swtp_mod_eint_enable(void) { return 0; }
int swtp_mod_eint_init(void)   { return 0; }
int swtp_mod_eint_read(void)   { return 0; }
#endif

