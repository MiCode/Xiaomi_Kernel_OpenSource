//#include "extmd_mt6252d.h"
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_gpio.h>
#include <linux/spinlock.h>
#include "cust_gpio_usage.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <mach/eint.h>
#include <linux/interrupt.h>
#else
#include <cust_eint.h>
#endif

#define EMD_MSG_INF(tag, fmt, args...)  printk(KERN_ERR "[emd/" tag "]" fmt, ##args)
#define POWER_ON_WAIT_RESET_TIME 1000
#define RESET_WAIT_RELEASE_TIME 10
#define POWER_ON_HOLD_TIME 5000

typedef struct _eint_inf{
    unsigned int gpio_id;
    unsigned int irq_id;
    unsigned int intr_flag;
    unsigned int debounce_time;
    unsigned int irq_en_cnt;
    unsigned int irq_reg;
    void (*eint_cb)(void);
    spinlock_t   lock;
}eint_inf_t;

extern void mt_eint_mask(unsigned int eint_num);
extern void mt_eint_unmask(unsigned int eint_num);
extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);

eint_inf_t ext_md_exp_eint;
eint_inf_t ext_md_wdt_eint;
eint_inf_t ext_md_wk_eint;
atomic_t traffic_on;
static const char *uart_port = "ttyMT1";			//should sync with user space
extern unsigned int mtk_uart_freeze_enable(char *port, int enable);

int get_eint_info(const char *name, eint_inf_t *eint_item)
{
#ifdef CONFIG_OF
    struct device_node *node;
    char buf[64];
    int  ints[2];

    if ((NULL==name) || (NULL==eint_item)) {
        EMD_MSG_INF("chr","%s: get invalid arguments:%p,%p\n", __func__, name, eint_item);
        return -1;
    }

    snprintf(buf, 64, "mediatek, %s-eint", name);
    EMD_MSG_INF("chr","%s: find:%s\n", __func__, buf);
    node = of_find_compatible_node(NULL, NULL, buf);
    if (node) {
        of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
        eint_item->gpio_id = ints[0];
        eint_item->debounce_time = ints[1];
        eint_item->irq_id = irq_of_parse_and_map(node, 0);
    } else {
        EMD_MSG_INF("chr","%s: can't find %s\n", __func__, name);
        return -2;
    }

#endif
    return 0;
}

int eint_var_init(void)
{
    memset(&ext_md_exp_eint, 0, sizeof(ext_md_exp_eint));
    spin_lock_init(&ext_md_exp_eint.lock);
    memset(&ext_md_wdt_eint, 0, sizeof(ext_md_wdt_eint));
    spin_lock_init(&ext_md_wdt_eint.lock);
    memset(&ext_md_wk_eint, 0, sizeof(ext_md_wk_eint));
    spin_lock_init(&ext_md_wk_eint.lock);

#ifdef CONFIG_OF
    EMD_MSG_INF("chr","%s: CONFIG_OF en\n", __func__);
    if(get_eint_info("DT_EXT_MD_EXP", &ext_md_exp_eint) != 0)
        return -1;
    if(get_eint_info("DT_EXT_MD_WDT", &ext_md_wdt_eint) != 0)
        return -1;
    if(get_eint_info("DT_EXT_MD_WK_UP", &ext_md_wk_eint) != 0)
        return -1;
#else
    EMD_MSG_INF("chr","%s: CONFIG_OF dis\n", __func__);
    // For exp
    ext_md_exp_eint.gpio_id = GPIO_EXT_MD_EXP;
    ext_md_exp_eint.debounce_time = CUST_EINT_DT_EXT_MD_EXP_DEBOUNCE_CN;
    ext_md_exp_eint.irq_id = CUST_EINT_DT_EXT_MD_EXP_NUM;
    ext_md_exp_eint.intr_flag = CUST_EINT_DT_EXT_MD_EXP_TYPE;
    // For wdt
    ext_md_wdt_eint.gpio_id = GPIO_EXT_MD_WD;
    ext_md_wdt_eint.debounce_time = CUST_EINT_DT_EXT_MD_WDT_DEBOUNCE_CN;
    ext_md_wdt_eint.irq_id = CUST_EINT_DT_EXT_MD_WDT_NUM;
    ext_md_wdt_eint.intr_flag = CUST_EINT_DT_EXT_MD_WDT_TYPE;
    // For wakeup
    ext_md_wk_eint.gpio_id = GPIO_EXT_MD_WK_AP;
    ext_md_wk_eint.debounce_time = CUST_EINT_DT_EXT_MD_WK_UP_DEBOUNCE_CN;
    ext_md_wk_eint.irq_id = CUST_EINT_DT_EXT_MD_WK_UP_NUM;
    ext_md_wk_eint.intr_flag = CUST_EINT_DT_EXT_MD_WK_UP_TYPE;
#endif

    EMD_MSG_INF("chr","%s: Eint-EXP(%d,%d,%d,%x)\n", __func__, ext_md_exp_eint.gpio_id, ext_md_exp_eint.debounce_time, \
               ext_md_exp_eint.irq_id, ext_md_exp_eint.intr_flag);
    EMD_MSG_INF("chr","%s: Eint-WDT(%d,%d,%d,%x)\n", __func__, ext_md_wdt_eint.gpio_id, ext_md_wdt_eint.debounce_time, \
               ext_md_wdt_eint.irq_id, ext_md_wdt_eint.intr_flag);
    EMD_MSG_INF("chr","%s: Eint-WK UP(%d,%d,%d,%x)\n", __func__, ext_md_wk_eint.gpio_id, ext_md_wk_eint.debounce_time, \
               ext_md_wk_eint.irq_id, ext_md_wk_eint.intr_flag);

    return 0;
}


void cm_enable_ext_md_wdt_irq(void)
{
    #ifdef CONFIG_OF
    unsigned long flags;
    #endif

    EMD_MSG_INF("chr","cm_enable_ext_md_wdt_irq:DT_EXT_MD_WD(%d)\n",ext_md_wdt_eint.irq_id);

    #ifdef CONFIG_OF
    spin_lock_irqsave(&ext_md_wdt_eint.lock, flags);
    if(ext_md_wdt_eint.irq_reg) {
        if(ext_md_wdt_eint.irq_en_cnt==0) {
            enable_irq(ext_md_wdt_eint.irq_id);
            ext_md_wdt_eint.irq_en_cnt = 1;
        }
    }
    spin_unlock_irqrestore(&ext_md_wdt_eint.lock, flags);
    #else
    mt_eint_unmask(ext_md_wdt_eint.irq_id);
    #endif
}

void cm_disable_ext_md_wdt_irq(void)
{
    #ifdef CONFIG_OF
    unsigned long flags;
    #endif

    EMD_MSG_INF("chr","cm_disable_ext_md_wdt_irq:CUST_EINT_DT_EXT_MD_WDT_NUM(%d)!\n",ext_md_wdt_eint.irq_id);

    #ifdef CONFIG_OF
    spin_lock_irqsave(&ext_md_wdt_eint.lock, flags);
    if(ext_md_wdt_eint.irq_reg) {
        if(ext_md_wdt_eint.irq_en_cnt>0) {
            disable_irq_nosync(ext_md_wdt_eint.irq_id);
            ext_md_wdt_eint.irq_en_cnt = 0;
        }
    }
    spin_unlock_irqrestore(&ext_md_wdt_eint.lock, flags);
    #else
    mt_eint_mask(ext_md_wdt_eint.irq_id);
    #endif
}

void cm_enable_ext_md_wakeup_irq(void)
{
    #ifdef CONFIG_OF
    unsigned long flags;
    #endif

    EMD_MSG_INF("chr","cm_enable_ext_md_wakeup_irq,CUST_EINT_DT_EXT_MD_WK_UP_NUM(%d)\n",ext_md_wk_eint.irq_id);

    #ifdef CONFIG_OF
    spin_lock_irqsave(&ext_md_wk_eint.lock, flags);
    if(ext_md_wk_eint.irq_reg) {
        if(ext_md_wk_eint.irq_en_cnt==0) {
            enable_irq(ext_md_wk_eint.irq_id);
            ext_md_wk_eint.irq_en_cnt = 1;
        }
    }
    spin_unlock_irqrestore(&ext_md_wk_eint.lock, flags);
    #else
    mt_eint_unmask(ext_md_wk_eint.irq_id);
    #endif
}

void cm_disable_ext_md_wakeup_irq(void)
{
    #ifdef CONFIG_OF
    unsigned long flags;
    #endif

    EMD_MSG_INF("chr","cm_disable_ext_md_wakeup_irq,CUST_EINT_DT_EXT_MD_WK_UP_NUM(%d)\n",ext_md_wk_eint.irq_id);

    #ifdef CONFIG_OF
    spin_lock_irqsave(&ext_md_wk_eint.lock, flags);
    if(ext_md_wk_eint.irq_reg) {
        if(ext_md_wk_eint.irq_en_cnt>0) {
            disable_irq_nosync(ext_md_wk_eint.irq_id);
            ext_md_wk_eint.irq_en_cnt = 0;
        }
    }
    spin_unlock_irqrestore(&ext_md_wk_eint.lock, flags);
    #else
    mt_eint_mask(ext_md_wk_eint.irq_id);
    #endif
}

void cm_enable_ext_md_exp_irq(void)
{
    #ifdef CONFIG_OF
    unsigned long flags;
    #endif

    EMD_MSG_INF("chr","cm_enable_ext_md_exp_irq,CUST_EINT_DT_EXT_MD_EXP_NUM(%d)\n",ext_md_exp_eint.irq_id);

    #ifdef CONFIG_OF
    spin_lock_irqsave(&ext_md_exp_eint.lock, flags);
    if(ext_md_exp_eint.irq_reg) {
        if(ext_md_exp_eint.irq_en_cnt==0) {
            enable_irq(ext_md_exp_eint.irq_id);
            ext_md_exp_eint.irq_en_cnt = 1;
        }
    }
    spin_unlock_irqrestore(&ext_md_exp_eint.lock, flags);
    #else
    mt_eint_unmask(ext_md_exp_eint.irq_id);
    #endif
}

void cm_disable_ext_md_exp_irq(void)
{
    #ifdef CONFIG_OF
    unsigned long flags;
    #endif

    EMD_MSG_INF("chr","cm_disable_ext_md_exp_irq,CUST_EINT_DT_EXT_MD_EXP_NUM(%d)\n",ext_md_exp_eint.irq_id);

    #ifdef CONFIG_OF
    spin_lock_irqsave(&ext_md_exp_eint.lock, flags);
    if(ext_md_exp_eint.irq_reg) {
        if(ext_md_exp_eint.irq_en_cnt>0) {
            disable_irq_nosync(ext_md_exp_eint.irq_id);
            ext_md_exp_eint.irq_en_cnt = 0;
        }
    }
    spin_unlock_irqrestore(&ext_md_exp_eint.lock, flags);
    #else
    mt_eint_mask(ext_md_exp_eint.irq_id);
    #endif
}


void cm_hold_rst_signal(void)
{
    EMD_MSG_INF("chr","cm_hold_rst_signal1:GPIO_EXT_MD_RST(%d)\n",mt_get_gpio_out(GPIO_EXT_MD_RST));
    mt_set_gpio_dir(GPIO_EXT_MD_RST, 1);
#ifdef GPIO_EXT_USB_SW2
    mt_set_gpio_out(GPIO_EXT_MD_RST, 0);
    EMD_MSG_INF("chr","cm_hold_rst_signal2:set evb GPIO_EXT_MD_RST(out)=%d!\n",mt_get_gpio_out(GPIO_EXT_MD_RST));
#else
    mt_set_gpio_out(GPIO_EXT_MD_RST, 1);
    EMD_MSG_INF("chr","cm_hold_rst_signal2:set phone GPIO_EXT_MD_RST(out)=%d!\n",mt_get_gpio_out(GPIO_EXT_MD_RST));
#endif
}
void cm_relese_rst_signal(void)
{
    EMD_MSG_INF("chr","cm_relese_rst_signal1:GPIO_EXT_MD_RST(%d)\n",mt_get_gpio_out(GPIO_EXT_MD_RST));
    mt_set_gpio_dir(GPIO_EXT_MD_RST, 1);
#ifdef GPIO_EXT_USB_SW2
    mt_set_gpio_out(GPIO_EXT_MD_RST, 1);
    EMD_MSG_INF("chr","cm_relese_rst_signal2:GPIO_EXT_MD_RST(%d)\n",mt_get_gpio_out(GPIO_EXT_MD_RST));
    mt_set_gpio_pull_enable(GPIO_EXT_MD_RST, 1);
    mt_set_gpio_dir(GPIO_EXT_MD_RST, 0);
    mt_set_gpio_pull_select(GPIO_EXT_MD_RST, 1);         
    EMD_MSG_INF("chr","cm_relese_rst_signal3:set evb GPIO_EXT_MD_RST(in)=%d!\n",mt_get_gpio_in(GPIO_EXT_MD_RST));    
#else
    mt_set_gpio_out(GPIO_EXT_MD_RST, 0);
    EMD_MSG_INF("chr","cm_relese_rst_signal3:set phoneGPIO_EXT_MD_RST(in)=%d!\n",mt_get_gpio_in(GPIO_EXT_MD_RST));
#endif
}
static int is_hold_rst=0;
static int ignore_wdt_interrupt = 0;
int cm_do_md_go(void)
{
    int ret = -1;
	if (is_hold_rst) 
	{   
	    is_hold_rst=0;
	    unsigned int retry = 100;
	    EMD_MSG_INF("chr","cm_do_md_go:1\n");
	    EMD_MSG_INF("chr","cm_do_md_go2:GPIO_EXT_MD_RST(out)=%d,GPIO_EXT_MD_WD(in)=%d\n",mt_get_gpio_out(GPIO_EXT_MD_RST),mt_get_gpio_in(GPIO_EXT_MD_WD));
	    cm_relese_rst_signal();
    	atomic_set(&traffic_on, 1);
	    EMD_MSG_INF("chr","cm_do_md_go3:GPIO_EXT_MD_RST(out)=%d,GPIO_EXT_MD_WD(in)=%d\n",mt_get_gpio_out(GPIO_EXT_MD_RST),mt_get_gpio_in(GPIO_EXT_MD_WD));
	    // Check WDT pin to high
	    while(retry>0){
	        retry--;
	        if(mt_get_gpio_in(GPIO_EXT_MD_WD)==0)
	        {
	            msleep(10);
	        }
	        else
	        {
	            ret=100-retry;
	            break;
	        }
	    }
	    EMD_MSG_INF("chr","cm_do_md_go4:GPIO_EXT_MD_RST(out)=%d,GPIO_EXT_MD_WD(in)=%d\n",mt_get_gpio_out(GPIO_EXT_MD_RST),mt_get_gpio_in(GPIO_EXT_MD_WD));
		msleep(POWER_ON_HOLD_TIME);
		mt_set_gpio_out(GPIO_EXT_MD_PWR_KEY, 0);
	}
	mt_set_gpio_dir(GPIO_EXT_MD_WK_AP, 0);
    mt_set_gpio_pull_enable(GPIO_EXT_MD_WK_AP, 1);
    mt_set_gpio_pull_select(GPIO_EXT_MD_WK_AP, 1);
    cm_enable_ext_md_wdt_irq();
    cm_enable_ext_md_wakeup_irq();
    cm_enable_ext_md_exp_irq();
    
    msleep(50);
    ignore_wdt_interrupt = 0;
    return ret;
}

#ifdef CONFIG_MTK_DT_USB_SUPPORT
#ifdef CONFIG_PM_RUNTIME
void usb11_auto_resume(void);
#endif
#endif
int cm_enter_md_download_mode(void)
{
#ifdef CONFIG_MTK_DT_USB_SUPPORT
#ifdef CONFIG_PM_RUNTIME
	/* make sure usb device tree is waked up so that usb is ready */
	usb11_auto_resume();
#endif
#endif
    EMD_MSG_INF("chr","cm_do_md_power_on:set GPIO_EXT_MD_DL_KEY(%d)\n",GPIO_EXT_MD_DL_KEY);
    // Press download key to let md can enter download mode
    mt_set_gpio_dir(GPIO_EXT_MD_DL_KEY, 1);
#ifdef GPIO_EXT_USB_SW2
    mt_set_gpio_out(GPIO_EXT_MD_DL_KEY, 0);
    EMD_MSG_INF("chr","cm_do_md_power_on:set evb GPIO_EXT_MD_DL_KEY=%d\n",mt_get_gpio_out(GPIO_EXT_MD_DL_KEY));
#else
    mt_set_gpio_out(GPIO_EXT_MD_DL_KEY, 1);
    EMD_MSG_INF("chr","cm_do_md_power_on:set phone GPIO_EXT_MD_DL_KEY=%d\n",mt_get_gpio_out(GPIO_EXT_MD_DL_KEY));
#endif
    mt_set_gpio_dir(GPIO_EXT_MD_WK_AP, 0);
    mt_set_gpio_pull_enable(GPIO_EXT_MD_WK_AP, 1);
    mt_set_gpio_pull_select(GPIO_EXT_MD_WK_AP, 0);
    
    // Press power key
    mt_set_gpio_dir(GPIO_EXT_MD_PWR_KEY, 1);
    mt_set_gpio_out(GPIO_EXT_MD_PWR_KEY, 1);
	msleep(POWER_ON_WAIT_RESET_TIME);
	ignore_wdt_interrupt = 1;
	cm_hold_rst_signal();
	msleep(RESET_WAIT_RELEASE_TIME);
	cm_relese_rst_signal();
	// Hold on
    msleep(POWER_ON_HOLD_TIME);

    cm_enable_ext_md_wdt_irq();
    cm_enable_ext_md_wakeup_irq();
    cm_enable_ext_md_exp_irq();
    return 0;
}

int cm_do_md_power_on(int bootmode)
{
    EMD_MSG_INF("chr","cm_do_md_power_on\n");    
    cm_disable_ext_md_wdt_irq();
    cm_disable_ext_md_wakeup_irq();
    cm_disable_ext_md_exp_irq();

    // Release download key to let md can enter normal boot
    mt_set_gpio_dir(GPIO_EXT_MD_DL_KEY, 1);
#ifdef GPIO_EXT_USB_SW2
    mt_set_gpio_out(GPIO_EXT_MD_DL_KEY, 1);
    EMD_MSG_INF("chr","cm_do_md_power_on:set evb GPIO_EXT_MD_DL_KEY=%d\n",mt_get_gpio_out(GPIO_EXT_MD_DL_KEY));
#else
    mt_set_gpio_out(GPIO_EXT_MD_DL_KEY, 0);
    EMD_MSG_INF("chr","cm_do_md_power_on:set phone GPIO_EXT_MD_DL_KEY=%d\n",mt_get_gpio_out(GPIO_EXT_MD_DL_KEY));
#endif

    mt_set_gpio_dir(GPIO_EXT_MD_PWR_KEY, 1);
    mt_set_gpio_out(GPIO_EXT_MD_PWR_KEY, 1);
		msleep(POWER_ON_WAIT_RESET_TIME);
		
	ignore_wdt_interrupt = 1;
	cm_hold_rst_signal();
    is_hold_rst = 1;
    EMD_MSG_INF("chr","cm_do_md_power_on:reset GPIO_EXT_MD_DL_KEY(%d),GPIO_EXT_MD_PWR_KEY(%d)\n",mt_get_gpio_out(GPIO_EXT_MD_DL_KEY),mt_get_gpio_out(GPIO_EXT_MD_PWR_KEY));
    
    if(bootmode)
    {
#ifndef  GPIO_EXT_USB_SW2
        mt_set_gpio_dir(GPIO_EXT_MD_META, 1);
        mt_set_gpio_out(GPIO_EXT_MD_META, 1);
        EMD_MSG_INF("chr","cm_do_md_power_on:meta mode,phone GPIO_EXT_MD_META(%d)\n",mt_get_gpio_out(GPIO_EXT_MD_META));
#else
        mt_set_gpio_pull_enable(GPIO_EXT_MD_DUMP, 1);
        mt_set_gpio_dir(GPIO_EXT_MD_DUMP, 1);
        mt_set_gpio_out(GPIO_EXT_MD_DUMP, 1);
        EMD_MSG_INF("chr","cm_do_md_power_on:meta mode,evb GPIO_EXT_MD_DUMP(%d)\n",mt_get_gpio_out(GPIO_EXT_MD_DUMP));
#endif
    }
    else
    {
#ifndef  GPIO_EXT_USB_SW2
        mt_set_gpio_dir(GPIO_EXT_MD_META, 1);
        mt_set_gpio_out(GPIO_EXT_MD_META, 0);
        EMD_MSG_INF("chr","cm_do_md_power_on,phone GPIO_EXT_MD_META(%d)\n",mt_get_gpio_out(GPIO_EXT_MD_META));
#else
        mt_set_gpio_dir(GPIO_EXT_MD_DUMP, 1);
        mt_set_gpio_out(GPIO_EXT_MD_DUMP, 0);
        EMD_MSG_INF("chr","cm_do_md_power_on,evb GPIO_EXT_MD_DUMP(%d)\n",mt_get_gpio_out(GPIO_EXT_MD_DUMP));
#endif
    }
    
    mt_set_gpio_dir(GPIO_EXT_MD_WK_AP, 0);
    mt_set_gpio_pull_enable(GPIO_EXT_MD_WK_AP, 1);
    mt_set_gpio_pull_select(GPIO_EXT_MD_WK_AP, 0);
    mt_set_gpio_dir(GPIO_EXT_MD_EXP, 0);
    mt_set_gpio_pull_enable(GPIO_EXT_MD_EXP, 1);
    mt_set_gpio_pull_select(GPIO_EXT_MD_EXP, 1);
    
    mtk_uart_freeze_enable(uart_port,1);
    
    msleep(RESET_WAIT_RELEASE_TIME);

    //For low power, we switch UART GPIO when power off md, so restore here according to dws default setting.
    mt_set_gpio_mode(GPIO_UART_URXD1_PIN, 1);
    mt_set_gpio_mode(GPIO_UART_UTXD1_PIN, 1);
    mt_set_gpio_mode(GPIO_UART_URTS1_PIN, 1);
    mt_set_gpio_mode(GPIO_UART_UCTS1_PIN, 1);
    mt_set_gpio_dir(GPIO_UART_URXD1_PIN, 0);
    mt_set_gpio_pull_enable(GPIO_UART_URXD1_PIN, 1);
    mt_set_gpio_pull_select(GPIO_UART_URXD1_PIN, 1);
    mt_set_gpio_dir(GPIO_UART_UTXD1_PIN, 1);
    mt_set_gpio_out(GPIO_UART_UTXD1_PIN, 1);
    mt_set_gpio_dir(GPIO_UART_URTS1_PIN, 1);
    mt_set_gpio_out(GPIO_UART_URTS1_PIN, 1);
    mt_set_gpio_dir(GPIO_UART_UCTS1_PIN, 0);
    mt_set_gpio_pull_enable(GPIO_UART_UCTS1_PIN, 1);
    mt_set_gpio_pull_select(GPIO_UART_UCTS1_PIN, 1);
    
    EMD_MSG_INF("chr","uart gpio restore\n");
	
    EMD_MSG_INF("chr","cm_do_md_power_on: GPIO_EXT_MD_EXP(%d),GPIO_EXT_MD_WD(%d),GPIO_EXT_MD_RST(%d),GPIO_EXT_MD_PWR_KEY(%d)\n", \
    mt_get_gpio_in(GPIO_EXT_MD_EXP),mt_get_gpio_in(GPIO_EXT_MD_WD),mt_get_gpio_out(GPIO_EXT_MD_RST),mt_get_gpio_out(GPIO_EXT_MD_PWR_KEY));

    return 0;
}
void cm_dump_gpio(void)
{
	unsigned int pin =0;
	pin=GPIO_EXT_MD_DL_KEY;
	EMD_MSG_INF("chr","cm_dump_gpio:GPIO_EXT_MD_DL_KEY: dir(%d) in(%d),out(%d)\n", \
		mt_get_gpio_dir(pin), mt_get_gpio_in(pin), mt_get_gpio_out(pin));  
  pin=GPIO_EXT_MD_PWR_KEY;
	EMD_MSG_INF("chr","cm_dump_gpio:GPIO_EXT_MD_PWR_KEY: dir(%d) in(%d),out(%d)\n", \
		mt_get_gpio_dir(pin), mt_get_gpio_in(pin), mt_get_gpio_out(pin));
	pin=GPIO_EXT_MD_RST;
	EMD_MSG_INF("chr","cm_dump_gpio:GPIO_EXT_MD_RST: dir(%d) in(%d),out(%d)\n", \
		mt_get_gpio_dir(pin), mt_get_gpio_in(pin), mt_get_gpio_out(pin));
  pin=GPIO_EXT_MD_WD;
	EMD_MSG_INF("chr","cm_dump_gpio:GPIO_EXT_MD_WD: dir(%d) in(%d),out(%d)\n", \
		mt_get_gpio_dir(pin), mt_get_gpio_in(pin), mt_get_gpio_out(pin));
  pin=GPIO_EXT_MD_WK_AP;
	EMD_MSG_INF("chr","cm_dump_gpio:GPIO_EXT_MD_WK_AP: dir(%d) in(%d),out(%d)\n", \
		mt_get_gpio_dir(pin), mt_get_gpio_in(pin), mt_get_gpio_out(pin));	
  pin=GPIO_EXT_AP_WK_MD;
	EMD_MSG_INF("chr","cm_dump_gpio:GPIO_EXT_AP_WK_MD: dir(%d) in(%d),out(%d)\n", \
		mt_get_gpio_dir(pin), mt_get_gpio_in(pin), mt_get_gpio_out(pin));		
  pin=GPIO_EXT_MD_EXP;
	EMD_MSG_INF("chr","cm_dump_gpio:GPIO_EXT_MD_EXP: dir(%d) in(%d),out(%d)\n", \
		mt_get_gpio_dir(pin), mt_get_gpio_in(pin), mt_get_gpio_out(pin));			
  pin=GPIO_EXT_MD_DUMP;
	EMD_MSG_INF("chr","cm_dump_gpio:GPIO_EXT_MD_DUMP: dir(%d) in(%d),out(%d)\n", \
		mt_get_gpio_dir(pin), mt_get_gpio_in(pin), mt_get_gpio_out(pin));
#ifndef  GPIO_EXT_USB_SW2
  pin=GPIO_EXT_MD_META;
	EMD_MSG_INF("chr","cm_dump_gpio:GPIO_EXT_MD_META: dir(%d) in(%d),out(%d)\n", \
		mt_get_gpio_dir(pin), mt_get_gpio_in(pin), mt_get_gpio_out(pin));
#endif
}
int cm_do_md_power_off(void)
{
    EMD_MSG_INF("chr","cm_do_md_power_off\n");
    atomic_set(&traffic_on, 0);
    // Release download key to let md can enter normal boot
    mt_set_gpio_dir(GPIO_EXT_MD_DL_KEY, 1);
#ifdef GPIO_EXT_USB_SW2
    mt_set_gpio_out(GPIO_EXT_MD_DL_KEY, 0);//set evb 
#else
    mt_set_gpio_out(GPIO_EXT_MD_DL_KEY, 1);//set phone
#endif
    
    cm_disable_ext_md_wdt_irq();
    cm_disable_ext_md_wakeup_irq();
    cm_disable_ext_md_exp_irq();      
    mt_set_gpio_dir(GPIO_EXT_AP_WK_MD, 1);
    mt_set_gpio_out(GPIO_EXT_AP_WK_MD, 0);    
#ifndef  GPIO_EXT_USB_SW2     
    mt_set_gpio_dir(GPIO_EXT_MD_META, 1);    
    mt_set_gpio_out(GPIO_EXT_MD_META, 0);
#endif 
    mt_set_gpio_out(GPIO_EXT_MD_PWR_KEY, 0);
    //EMD_MSG_INF("chr","cm_do_md_power_off:GPIO_EXT_MD_PWR_KEY(%d)2\n",mt_get_gpio_out(GPIO_EXT_MD_PWR_KEY));

    mt_set_gpio_dir(GPIO_EXT_MD_WK_AP, 0);
    mt_set_gpio_pull_enable(GPIO_EXT_MD_WK_AP, 1);
    mt_set_gpio_pull_select(GPIO_EXT_MD_WK_AP, 0);
    mt_set_gpio_dir(GPIO_EXT_MD_EXP, 0);
    mt_set_gpio_pull_enable(GPIO_EXT_MD_EXP, 1);
    mt_set_gpio_pull_select(GPIO_EXT_MD_EXP, 0);
    
    mt_set_gpio_dir(GPIO_EXT_MD_DUMP, 0);
    mt_set_gpio_pull_enable(GPIO_EXT_MD_DUMP, 1);
    mt_set_gpio_pull_select(GPIO_EXT_MD_DUMP, 0);
    
    //For low power, we switch UART GPIO when power off md
    mt_set_gpio_mode(GPIO_UART_URXD1_PIN, 0);
    mt_set_gpio_mode(GPIO_UART_UTXD1_PIN, 0);
    mt_set_gpio_mode(GPIO_UART_URTS1_PIN, 0);
    mt_set_gpio_mode(GPIO_UART_UCTS1_PIN, 0);
    mt_set_gpio_dir(GPIO_UART_URXD1_PIN, 0);
    mt_set_gpio_pull_enable(GPIO_UART_URXD1_PIN, 1);
    mt_set_gpio_pull_select(GPIO_UART_URXD1_PIN, 0);
    mt_set_gpio_dir(GPIO_UART_UTXD1_PIN, 0);
    mt_set_gpio_pull_enable(GPIO_UART_UTXD1_PIN, 1);
    mt_set_gpio_pull_select(GPIO_UART_UTXD1_PIN, 0);
    mt_set_gpio_dir(GPIO_UART_URTS1_PIN, 0);
    mt_set_gpio_pull_enable(GPIO_UART_URTS1_PIN, 1);
    mt_set_gpio_pull_select(GPIO_UART_URTS1_PIN, 0);
    mt_set_gpio_dir(GPIO_UART_UCTS1_PIN, 0);
    mt_set_gpio_pull_enable(GPIO_UART_UCTS1_PIN, 1);
    mt_set_gpio_pull_select(GPIO_UART_UCTS1_PIN, 0);
    
    EMD_MSG_INF("chr","uart gpio pull down\n");
    
    cm_dump_gpio();
    return 0;
}

void cm_hold_wakeup_md_signal(void)
{
    EMD_MSG_INF("chr","cm_hold_wakeup_md_signal:GPIO_EXT_AP_WK_MD=%d,0!\n",mt_get_gpio_out(GPIO_EXT_AP_WK_MD));
    mt_set_gpio_out(GPIO_EXT_AP_WK_MD, 0);
}

void cm_release_wakeup_md_signal(void)
{    
    mt_set_gpio_out(GPIO_EXT_AP_WK_MD, 1);
    EMD_MSG_INF("chr","cm_release_wakeup_md_signal:GPIO_EXT_AP_WK_MD=%d,1!\n",mt_get_gpio_out(GPIO_EXT_AP_WK_MD));
}

int is_traffic_on(int type)
{
	return atomic_read(&traffic_on);
}

void cm_gpio_setup(void)
{
    EMD_MSG_INF("chr","cm_gpio_setup 1\n");
    atomic_set(&traffic_on, 0);
    // MD wake up AP pin
    mt_set_gpio_pull_enable(GPIO_EXT_MD_WK_AP, 1);
    mt_set_gpio_pull_select(GPIO_EXT_MD_WK_AP, 0);
    mt_set_gpio_dir(GPIO_EXT_MD_WK_AP, 0);
    mt_set_gpio_mode(GPIO_EXT_MD_WK_AP, GPIO_EXT_MD_WK_AP_M_EINT); 
    EMD_MSG_INF("chr","cm_gpio_setup:GPIO_EXT_MD_WK_AP=%d\n",mt_get_gpio_out(GPIO_EXT_MD_WK_AP));
    // AP wake up MD pin
    mt_set_gpio_mode(GPIO_EXT_AP_WK_MD, GPIO_EXT_AP_WK_MD_M_GPIO); // GPIO Mode
    mt_set_gpio_dir(GPIO_EXT_AP_WK_MD, 1);
    mt_set_gpio_out(GPIO_EXT_AP_WK_MD, 0);
		EMD_MSG_INF("chr","cm_gpio_setup:GPIO_EXT_AP_WK_MD=%d\n",mt_get_gpio_out(GPIO_EXT_AP_WK_MD));
    // Rest MD pin
    mt_set_gpio_mode(GPIO_EXT_MD_RST, GPIO_EXT_MD_RST_M_GPIO); //GPIO202 is reset pin
    mt_set_gpio_pull_enable(GPIO_EXT_MD_RST, 0);
    mt_set_gpio_pull_select(GPIO_EXT_MD_RST, 1);
    cm_relese_rst_signal();
    EMD_MSG_INF("chr","cm_gpio_setup 4\n");
		EMD_MSG_INF("chr","cm_gpio_setup:GPIO_EXT_MD_RST=%d\n",mt_get_gpio_out(GPIO_EXT_MD_RST));
    // MD power key pin
    mt_set_gpio_mode(GPIO_EXT_MD_PWR_KEY, GPIO_EXT_MD_PWR_KEY_M_GPIO); //GPIO 200 is power key
    mt_set_gpio_pull_enable(GPIO_EXT_MD_PWR_KEY, 0);
    mt_set_gpio_dir(GPIO_EXT_MD_PWR_KEY, 1);
    mt_set_gpio_out(GPIO_EXT_MD_PWR_KEY, 0);
		EMD_MSG_INF("chr","cm_gpio_setup:GPIO_EXT_MD_PWR_KEY=%d\n",mt_get_gpio_out(GPIO_EXT_MD_PWR_KEY));
    // MD WDT irq pin
    mt_set_gpio_pull_enable(GPIO_EXT_MD_WD, 1);
    mt_set_gpio_pull_select(GPIO_EXT_MD_WD, 1);
    mt_set_gpio_dir(GPIO_EXT_MD_WD, 0);
    mt_set_gpio_mode(GPIO_EXT_MD_WD, GPIO_EXT_MD_WD_M_EINT); // EINT168
    EMD_MSG_INF("chr","cm_gpio_setup GPIO_EXT_MD_WD(in)=%d\n",mt_get_gpio_in(GPIO_EXT_MD_WD));

    // MD Exception irq pin
    mt_set_gpio_pull_enable(GPIO_EXT_MD_EXP, 1);
    mt_set_gpio_pull_select(GPIO_EXT_MD_EXP, 1);
    mt_set_gpio_dir(GPIO_EXT_MD_EXP, 0);
    mt_set_gpio_mode(GPIO_EXT_MD_EXP, GPIO_EXT_MD_EXP_M_EINT); //     
    EMD_MSG_INF("chr","cm_gpio_setup GPIO_EXT_MD_EXP(in)=%d\n",mt_get_gpio_in(GPIO_EXT_MD_EXP));
#ifndef  GPIO_EXT_USB_SW2     
    mt_set_gpio_mode(GPIO_EXT_MD_META, GPIO_EXT_MD_META_M_GPIO); 
    mt_set_gpio_dir(GPIO_EXT_MD_META, 1);// Using input floating    
    mt_set_gpio_out(GPIO_EXT_MD_META, 0);// Default @ reset state
    EMD_MSG_INF("chr","cm_gpio_setup:phone GPIO_EXT_MD_META=%d\n",mt_get_gpio_out(GPIO_EXT_MD_META));
#else
    mt_set_gpio_pull_enable(GPIO_EXT_MD_DUMP, 1);
    mt_set_gpio_pull_select(GPIO_EXT_MD_DUMP, 0); 
    mt_set_gpio_dir(GPIO_EXT_MD_DUMP, 1);  
    mt_set_gpio_mode(GPIO_EXT_MD_DUMP, GPIO_EXT_MD_DUMP_M_GPIO);
    EMD_MSG_INF("chr","cm_gpio_setup:evb GPIO_EXT_MD_DUMP(in)=(%d)\n",mt_get_gpio_in(GPIO_EXT_MD_DUMP));
#endif

    // Configure eint
    eint_var_init();
}

void cm_ext_md_rst(void)
{
    EMD_MSG_INF("chr","TODO: Dummy cm_ext_md_rst!\n");
}

irqreturn_t ext_md_exp_eint_handler(unsigned irq, struct irq_desc *desc)
{
    if(ext_md_exp_eint.eint_cb)
        ext_md_exp_eint.eint_cb();

    if(ext_md_exp_eint.irq_en_cnt) {
        disable_irq_nosync(irq);
        ext_md_exp_eint.irq_en_cnt = 0;
    }

    return IRQ_HANDLED;
}

irqreturn_t ext_md_wk_eint_handler(unsigned irq, struct irq_desc *desc)
{
    if(ext_md_wk_eint.eint_cb)
        ext_md_wk_eint.eint_cb();

    if(ext_md_wk_eint.irq_en_cnt) {
        disable_irq_nosync(irq);
        ext_md_wk_eint.irq_en_cnt = 0;
    }

    return IRQ_HANDLED;
}

irqreturn_t ext_md_wdt_eint_handler(unsigned irq, struct irq_desc *desc)
{
    if (ignore_wdt_interrupt){
    	EMD_MSG_INF("chr","emd_wdt-eint ignored\n");
    	return IRQ_HANDLED;
    }
    if(ext_md_wdt_eint.eint_cb)
        ext_md_wdt_eint.eint_cb();

    if(ext_md_wdt_eint.irq_en_cnt) {
        disable_irq_nosync(irq);
        ext_md_wdt_eint.irq_en_cnt = 0;
    }
    return IRQ_HANDLED;
}

int cm_register_irq_cb(int type, void(*irq_cb)(void))
{
    #ifdef CONFIG_OF
    unsigned long flags;
    int ret=0;
    #endif

    switch(type)
    {
        case 0:
            //--- Ext MD wdt irq -------
            #ifdef CONFIG_OF
            mt_gpio_set_debounce(ext_md_wdt_eint.irq_id, ext_md_wdt_eint.debounce_time*1000);
            spin_lock_irqsave(&ext_md_wdt_eint.lock, flags);
            ext_md_wdt_eint.eint_cb = irq_cb;
            ext_md_wdt_eint.irq_reg = 1;
            ext_md_wdt_eint.irq_en_cnt=1;
            spin_unlock_irqrestore(&ext_md_wdt_eint.lock, flags);
            ret = request_irq(ext_md_wdt_eint.irq_id, ext_md_wdt_eint_handler, IRQF_TRIGGER_NONE, "emd_wdt-eint", NULL);
            if(ret != 0) {
                EMD_MSG_INF("chr","emd_wdt-eint register fail\n");
                spin_lock_irqsave(&ext_md_wdt_eint.lock, flags);
                ext_md_wdt_eint.eint_cb = NULL;
                ext_md_wdt_eint.irq_reg = 0;
                ext_md_wdt_eint.irq_en_cnt=0;
                spin_unlock_irqrestore(&ext_md_wdt_eint.lock, flags);
            }
            #else
            cm_disable_ext_md_wdt_irq();
            mt_eint_set_hw_debounce(ext_md_wdt_eint.irq_id, ext_md_wdt_eint.debounce_time);
            mt_eint_registration(ext_md_wdt_eint.irq_id,  ext_md_wdt_eint.intr_flag, irq_cb, 0);
            #endif
            break;
        case 1:
            //--- Ext MD wake up irq ------------
            #ifdef CONFIG_OF
            mt_gpio_set_debounce(ext_md_wk_eint.irq_id, ext_md_wk_eint.debounce_time*1000);
            spin_lock_irqsave(&ext_md_wk_eint.lock, flags);
            ext_md_wk_eint.eint_cb = irq_cb;
            ext_md_wk_eint.irq_reg = 1;
            ext_md_wk_eint.irq_en_cnt=1;
            spin_unlock_irqrestore(&ext_md_wk_eint.lock, flags);
            ret = request_irq(ext_md_wk_eint.irq_id, ext_md_wk_eint_handler, IRQF_TRIGGER_NONE, "emd_wkup-eint", NULL);
            if(ret != 0) {
                EMD_MSG_INF("chr","emd_wkup-eint register fail\n");
                spin_lock_irqsave(&ext_md_wk_eint.lock, flags);
                ext_md_wk_eint.eint_cb = NULL;
                ext_md_wk_eint.irq_reg = 0;
                ext_md_wk_eint.irq_en_cnt=0;
                spin_unlock_irqrestore(&ext_md_wk_eint.lock, flags);
            }
            #else
            cm_disable_ext_md_wakeup_irq();
            mt_eint_set_hw_debounce(ext_md_wk_eint.irq_id, ext_md_wk_eint.debounce_time);
            mt_eint_registration(ext_md_wk_eint.irq_id, ext_md_wk_eint.intr_flag, irq_cb, 0);
            #endif
            break;
        case 2:
            //--- Ext MD exception  irq ------------
            #ifdef CONFIG_OF
            mt_gpio_set_debounce(ext_md_exp_eint.irq_id, ext_md_exp_eint.debounce_time*1000);
            spin_lock_irqsave(&ext_md_exp_eint.lock, flags);
            ext_md_exp_eint.eint_cb = irq_cb;
            ext_md_exp_eint.irq_reg = 1;
            ext_md_exp_eint.irq_en_cnt=1;
            spin_unlock_irqrestore(&ext_md_exp_eint.lock, flags);
            ret = request_irq(ext_md_exp_eint.irq_id, ext_md_exp_eint_handler, IRQF_TRIGGER_NONE, "emd_exp-eint", NULL);
            if(ret != 0) {
                EMD_MSG_INF("chr","emd_exp-eint register fail\n");
                spin_lock_irqsave(&ext_md_exp_eint.lock, flags);
                ext_md_exp_eint.eint_cb = NULL;
                ext_md_exp_eint.irq_reg = 0;
                ext_md_exp_eint.irq_en_cnt=0;
                spin_unlock_irqrestore(&ext_md_exp_eint.lock, flags);
            }
            #else
            cm_disable_ext_md_exp_irq();
            mt_eint_set_hw_debounce(ext_md_exp_eint.irq_id, ext_md_exp_eint.debounce_time);
            mt_eint_registration(ext_md_exp_eint.irq_id, ext_md_exp_eint.intr_flag, irq_cb, 0);            
            #endif
            break;
        default:
            EMD_MSG_INF("chr","No support irq!!\n");
            break;
    }

    return 0;
}

int cm_get_assertlog_status(void)
{
    int val;
    mt_set_gpio_dir(GPIO_EXT_MD_DUMP, 0); 
    val = mt_get_gpio_in(GPIO_EXT_MD_DUMP);    
    EMD_MSG_INF("chr","cm_get_assertlog_status:GPIO_EXT_MD_EXP(in)=%d!\n",val);
    return val;
}




