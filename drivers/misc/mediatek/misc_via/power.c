/*
 * viatel_cbp_power.c
 *
 * VIA CBP driver for Linux
 *
 * Copyright (C) 2009 VIA TELECOM Corporation, Inc.
 * Author: VIA TELECOM Corporation, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.  */
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include "viatel.h"
#include "core.h"
#ifdef OEM_HAVE_VOLT_PROTECTION/*take care,it should be modified if platform if changed*/
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <cust_gpio_boot.h>
#endif
//add by yfu to control LDO VGP2
#include <mach/mt_pm_ldo.h>


/* ioctl for vomodem, which must be same as viatelutils.h  */
#define VMDM_IOCTL_RESET	_IO( 'v', 0x01)
#define VMDM_IOCTL_POWER	_IOW('v', 0x02, int)
#define VMDM_IOCTL_CRL	_IOW('v', 0x03, int)
#define VMDM_IOCTL_DIE		_IO( 'v', 0x04)
#define VMDM_IOCTL_WAKE		_IO( 'v', 0x05)
#define VMDM_IOCTL_IGNORE		_IOW( 'v', 0x06, int)

/* event for vmodem, which must be same as viatelutilis.h */
enum ASC_USERSPACE_NOTIFIER_CODE{
    ASC_USER_USB_WAKE =  (__SI_POLL|100),
    ASC_USER_USB_SLEEP,
    ASC_USER_UART_WAKE,
    ASC_USER_UART_SLEEP,
    ASC_USER_SDIO_WAKE,
    ASC_USER_SDIO_SLEEP,
    ASC_USER_MDM_POWER_ON = (__SI_POLL|200),
    ASC_USER_MDM_POWER_OFF,
    ASC_USER_MDM_RESET_ON,
    ASC_USER_MDM_RESET_OFF,
	ASC_USER_MDM_ERR = (__SI_POLL|300),
	ASC_USER_MDM_ERR_ENHANCE
};

#define MDM_RST_LOCK_TIME   (120) 
#define MDM_RST_HOLD_DELAY  (100) //ms
#define MDM_PWR_HOLD_DELAY  (500) //ms

struct viatel_modem_data {
    struct fasync_struct *fasync;
    struct kobject *modem_kobj;
    struct raw_notifier_head ntf;
    struct notifier_block rst_ntf;
    struct notifier_block pwr_ntf;
	struct notifier_block err_ntf;
    struct wake_lock wlock;
    struct work_struct work;
    atomic_t count;
    unsigned long ntf_flags;
};

static struct viatel_modem_data *vmdata;
static unsigned char via_ignore_notifier = 0;

extern int modem_on_off_ctrl_chan(unsigned char on);
extern void gpio_irq_cbp_rst_ind(void);

#ifdef OEM_HAVE_VOLT_PROTECTION/*take care,it should be modified if platform if changed*/
static void via_vbat_ctl_gpio_init(void)
{
	mt_set_gpio_mode(GPIOEXT20, GPIO_MODE_GPIO);
	mt_set_gpio_mode(GPIOEXT21, GPIO_MODE_GPIO);
	mt_set_gpio_mode(GPIOEXT22, GPIO_MODE_GPIO);
	mt_set_gpio_dir(GPIOEXT20, GPIO_DIR_OUT);
	mt_set_gpio_dir(GPIOEXT21, GPIO_DIR_OUT);
	mt_set_gpio_dir(GPIOEXT22, GPIO_DIR_OUT);
}
void via_low_vol_protect(void)
{
	//changg to low vol protect
	mt_set_gpio_out(GPIOEXT20, GPIO_OUT_ONE);
	msleep(5);
	mt_set_gpio_out(GPIOEXT22, GPIO_OUT_ZERO);
	msleep(5);
	mt_set_gpio_out(GPIOEXT21, GPIO_OUT_ZERO);
}
static void via_sync_off(void)
{
	printk("via_sync_off power off modem vbat \n");
	msleep(50);
	mt_set_gpio_out(GPIOEXT22, GPIO_OUT_ZERO);
	msleep(5);
	mt_set_gpio_out(GPIOEXT21, GPIO_OUT_ZERO);
	msleep(5);
	mt_set_gpio_out(GPIOEXT20, GPIO_OUT_ZERO);
	msleep(5);
}

static void via_sync_on(void)
{
	printk("VIA_BAT Power on\n");
	via_vbat_ctl_gpio_init();
	via_low_vol_protect();	      
}
#endif

static void modem_signal_user(int event)
{
    if(vmdata && vmdata->fasync){
        printk("%s: evnet %d.\n",__FUNCTION__, (short)event);
        kill_fasync(&vmdata->fasync, SIGIO, event);
    }
}

/* Protection for the above */
static DEFINE_RAW_SPINLOCK(rslock);

void oem_reset_modem(void)
{
   wake_lock_timeout(&vmdata->wlock, MDM_RST_LOCK_TIME * HZ);
   if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST)){
	   oem_gpio_direction_output(GPIO_VIATEL_CRASH_CBP, 1);
       oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 1);
       mdelay(MDM_RST_HOLD_DELAY);
       oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 0);
       mdelay(MDM_RST_HOLD_DELAY);
   }
   printk("Warnning: reset vmodem\n");

}
EXPORT_SYMBOL(oem_reset_modem);

void oem_power_on_modem(void)
{
#ifdef OEM_HAVE_VOLT_PROTECTION/*take care,it should be modified if platform if changed*/
   via_sync_on();
#endif

   //add by yfu to control LDO VGP2
   //turn on VGP2 and set 2.8v
   // hwPowerOn(MT6323_POWER_LDO_VGP2, VOL_2800,"VIA");


   if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_EN)){
      if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST)){
		  oem_gpio_direction_output(GPIO_VIATEL_CRASH_CBP, 1);
          oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 0);
          mdelay(MDM_RST_HOLD_DELAY);
      }
      oem_gpio_direction_output(GPIO_VIATEL_MDM_PWR_EN, 1);
      mdelay(MDM_PWR_HOLD_DELAY);
   }
   printk("Warnning: power on vmodem\n");

}
EXPORT_SYMBOL(oem_power_on_modem);

void oem_power_off_modem(void)
{
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_EN)){
        oem_gpio_direction_output(GPIO_VIATEL_MDM_PWR_EN, 0);
    }

    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST)){
        oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 1);
        /*just hold the reset pin if no power enable pin*/
        if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_EN)){
            mdelay(MDM_RST_HOLD_DELAY);
            oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 0);
        }
    }
    //add by yfu to control LDO VGP2
    //turn off VGP2
    // hwPowerDown(MT6323_POWER_LDO_VGP2, "VIA");
    
#ifdef OEM_HAVE_VOLT_PROTECTION/*take care,it should be modified if platform if changed*/
    via_sync_off();
#endif
    printk("Warnning: power off vmodem\n");
}
EXPORT_SYMBOL(oem_power_off_modem);

ssize_t modem_power_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int power = 0;
    int ret = 0;
    
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_IND)){
        power = !!oem_gpio_get_value(GPIO_VIATEL_MDM_PWR_IND);
    }else if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_EN)){
        printk("No MDM_PWR_IND, just detect MDM_PWR_EN\n");
        power = !!oem_gpio_get_value(GPIO_VIATEL_MDM_PWR_EN);
    }else if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST)){
        printk("No MDM_PWR_IND, just detect MDM_PWR_RST\n");
        power = !!oem_gpio_get_value(GPIO_VIATEL_MDM_RST);
    }
    if(power){
        ret += sprintf(buf + ret, "on\n");
    }else{
        ret += sprintf(buf + ret, "off\n");
    }
    
    return ret;
}

ssize_t modem_power_store(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t n)
{
    int power;

    /* power the modem */
    if ( !strncmp(buf, "on", strlen("on"))) {
        power = 1;
    }else if(!strncmp(buf, "off", strlen("off"))){
        power = 0;
    }else{
        printk("%s: input %s is invalid.\n", __FUNCTION__, buf);
        return n;
    }

    if(power){
        oem_power_on_modem();
    }else{
        oem_power_off_modem();
    }
    return n;
}

ssize_t modem_reset_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int reset = 0;
    int ret = 0;
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST_IND)){
        reset = !!oem_gpio_get_value(GPIO_VIATEL_MDM_RST_IND);
    }else if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST)){
        reset = !!oem_gpio_get_value(GPIO_VIATEL_MDM_RST);
    }
    
    if(reset){
        ret += sprintf(buf + ret, "reset\n");
    }else{
        ret += sprintf(buf + ret, "work\n");
    }

    return ret;
}

ssize_t modem_reset_store(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t n)
{
    /* reset the modem */
    oem_reset_modem();
    return n;
}

ssize_t modem_ets_select_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int level = 0;
    int ret = 0;
    
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_ETS_SEL)){
        level = !!oem_gpio_get_value(GPIO_VIATEL_MDM_ETS_SEL);
    }

    ret += sprintf(buf, "%d\n", level);
    return ret;
}

ssize_t modem_ets_select_store(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t n)
{
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_ETS_SEL)){
        if ( !strncmp(buf, "1", strlen("1"))) {
            oem_gpio_direction_output(GPIO_VIATEL_MDM_ETS_SEL, 1);
        }else if( !strncmp(buf, "0", strlen("0"))){
            oem_gpio_direction_output(GPIO_VIATEL_MDM_ETS_SEL, 0);
        }else{
            printk("Unknow command.\n");
        }
    }

    return n;
}

ssize_t modem_boot_select_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int ret = 0;
    int level = 0;
    
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_BOOT_SEL)){
        level = !!oem_gpio_get_value(GPIO_VIATEL_MDM_BOOT_SEL);
    }

    ret += sprintf(buf, "%d\n", level);
    return ret;
}

ssize_t modem_boot_select_store(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t n)
{
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_BOOT_SEL)){
        if ( !strncmp(buf, "1", strlen("1"))) {
            oem_gpio_direction_output(GPIO_VIATEL_MDM_BOOT_SEL, 1);
        }else if( !strncmp(buf, "0", strlen("0"))){
            oem_gpio_direction_output(GPIO_VIATEL_MDM_BOOT_SEL, 0);
        }else{
            printk("Unknow command.\n");
        }
    }

    return n;
}
int modem_err_indication_usr(int revocery)
{
	printk("%s %d revocery=%d\n",__func__,__LINE__,revocery);
	if(revocery){
		printk("%s %d MDM_EVT_NOTIFY_HD_ERR\n",__func__,__LINE__);
		modem_notify_event(MDM_EVT_NOTIFY_HD_ERR);
	}
	else{
		printk("%s %d MDM_EVT_NOTIFY_HD_ENHANCE\n",__func__,__LINE__);
		modem_notify_event(MDM_EVT_NOTIFY_HD_ENHANCE);
	}
	return 0;
}
EXPORT_SYMBOL(modem_err_indication_usr);

void oem_let_cbp_die(void)
{
	if(GPIO_OEM_VALID(GPIO_VIATEL_CRASH_CBP)){
		oem_gpio_direction_output(GPIO_VIATEL_CRASH_CBP, 0);
		mdelay(MDM_RST_HOLD_DELAY);
		oem_gpio_direction_output(GPIO_VIATEL_CRASH_CBP, 1);
	}
	printk("let cbp die\n");
}
EXPORT_SYMBOL(oem_let_cbp_die);

ssize_t modem_diecbp_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int ret = 0;
    int level = 0;
    
    if(GPIO_OEM_VALID(GPIO_VIATEL_CRASH_CBP)){
        level = !!oem_gpio_get_value(GPIO_VIATEL_CRASH_CBP);
    }

    ret += sprintf(buf, "%d\n", level);
    return ret;
}

ssize_t modem_diecbp_store(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t n)
{
    if(GPIO_OEM_VALID(GPIO_VIATEL_CRASH_CBP)){
        if ( !strncmp(buf, "1", strlen("1"))) {
            oem_gpio_direction_output(GPIO_VIATEL_CRASH_CBP, 1);
        }else if( !strncmp(buf, "0", strlen("0"))){
            oem_gpio_direction_output(GPIO_VIATEL_CRASH_CBP, 0);
        }else{
            printk("Unknow command.\n");
        }
    }else{
		printk("invalid gpio.\n");
    }

    return n;
}
ssize_t modem_hderr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int ret = 0;
    int level = 0;
    
    if(GPIO_OEM_VALID(GPIO_VIATEL_CRASH_CBP)){
        level = !!oem_gpio_get_value(GPIO_VIATEL_CRASH_CBP);
    }

    ret += sprintf(buf, "%d\n", level);
    return ret;
}

ssize_t modem_hderr_store(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t n)
{
	printk("signal modem_err_indication_usr\n");
	if ( !strncmp(buf, "1", strlen("1"))) {
		modem_err_indication_usr(1);
	}else if( !strncmp(buf, "0", strlen("0"))){
		modem_err_indication_usr(0);
	}else{
		printk("Unknow command.\n");
	}


    return n;
}


static int modem_reset_notify_misc(struct notifier_block *nb, unsigned long event,
			       void *ptr)
{
    int ret = 0;
	if(via_ignore_notifier)
	{
		printk("Warnning: via ignore notifer just return NOTIFY_OK.\n");
		return NOTIFY_OK;
	}
	switch (event) {
        case MDM_EVT_NOTIFY_RESET_ON:
            modem_signal_user(ASC_USER_MDM_RESET_ON);
            break;
        case MDM_EVT_NOTIFY_RESET_OFF:
            modem_signal_user(ASC_USER_MDM_RESET_OFF);
            break;
        default:
            ;
    }

    return ret ? NOTIFY_DONE : NOTIFY_OK;
}

static int modem_power_notify_misc(struct notifier_block *nb, unsigned long event,
			       void *ptr)
{
    switch (event) {
        case MDM_EVT_NOTIFY_POWER_ON:
            modem_signal_user(ASC_USER_MDM_POWER_ON);
            break;
        case MDM_EVT_NOTIFY_POWER_OFF:
            modem_signal_user(ASC_USER_MDM_POWER_OFF);
            break;
        
        default:
            ;
    }

    return NOTIFY_OK;
}

static int modem_err_notify_misc(struct notifier_block *nb, unsigned long event,
			       void *ptr)
{
	printk("%s %d event=%ld\n",__func__,__LINE__,event);
    switch (event) {
        case MDM_EVT_NOTIFY_HD_ERR:
			printk("%s %d ASC_USER_MDM_ERR\n",__func__,__LINE__);
            modem_signal_user(ASC_USER_MDM_ERR);
            break;
        case MDM_EVT_NOTIFY_HD_ENHANCE:
			printk("%s %d ASC_USER_MDM_ERR_ENHANCE\n",__func__,__LINE__);
            modem_signal_user(ASC_USER_MDM_ERR_ENHANCE);
            break;
        
        default:
            ;
    }

    return NOTIFY_OK;
}



#define modem_attr(_name) \
static struct kobj_attribute _name##_attr = { \
    .attr = {                        \
        .name = __stringify(_name),   \
        .mode = 0640,                \
    },                               \
    .show   = modem_##_name##_show,  \
    .store  = modem_##_name##_store, \
}

modem_attr(power);
modem_attr(reset);
modem_attr(ets_select);
modem_attr(boot_select);
modem_attr(diecbp);
modem_attr(hderr);

static struct attribute *g_attr[] = {
    &power_attr.attr,
    &reset_attr.attr,
    &ets_select_attr.attr,
    &boot_select_attr.attr,
    &diecbp_attr.attr,
    &hderr_attr.attr,
    NULL
};

static struct attribute_group g_attr_group = {
    .attrs = g_attr,
};


static void modem_shutdown(struct platform_device *dev)
{
    oem_power_off_modem();
}

/*
 * Notify about a modem event change.
 * 
 */
static void modem_notify_task(struct work_struct *work)
{
    int i = 0;

    for(i = 0; i < MDM_EVT_NOTIFY_NUM; i++){
        if(test_and_clear_bit(i, &vmdata->ntf_flags)){
            raw_notifier_call_chain(&vmdata->ntf, i, NULL);
        }
    }
}

void modem_notify_event(int event)
{
    if(vmdata && event < MDM_EVT_NOTIFY_NUM){
        set_bit(event, &vmdata->ntf_flags);
        schedule_work(&vmdata->work);
    }
}
EXPORT_SYMBOL(modem_notify_event);

/*
 *  register a modem events change listener
 */
int modem_register_notifier(struct notifier_block *nb)
{
	int ret = -ENODEV;
	unsigned long flags;

    if(vmdata){
	    raw_spin_lock_irqsave(&rslock, flags);
	    ret = raw_notifier_chain_register(&vmdata->ntf, nb);
	    raw_spin_unlock_irqrestore(&rslock, flags);
    }
	return ret;
}
EXPORT_SYMBOL(modem_register_notifier);

/*
 *  unregister a modem events change listener
 */
int modem_unregister_notifier(struct notifier_block *nb)
{
	int ret = -ENODEV;
	unsigned long flags;

    if(vmdata){
	    raw_spin_lock_irqsave(&rslock, flags);
	    ret = raw_notifier_chain_unregister(&vmdata->ntf, nb);
	    raw_spin_unlock_irqrestore(&rslock, flags);
    }
	return ret;
}
EXPORT_SYMBOL(modem_unregister_notifier);


static irqreturn_t modem_reset_indication_irq(int irq, void *data)
{
    printk("%s %d \n",__FUNCTION__,__LINE__);
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST_IND)){
        oem_gpio_set_irq_type(GPIO_VIATEL_MDM_RST_IND, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING);
        if(oem_gpio_get_value(GPIO_VIATEL_MDM_RST_IND)){
            printk("%s %d ON\n",__FUNCTION__,__LINE__);
			wake_lock_timeout(&vmdata->wlock, MDM_RST_LOCK_TIME * HZ);
            modem_notify_event(MDM_EVT_NOTIFY_RESET_ON);
        }else{
            printk("%s %d OFF\n",__FUNCTION__,__LINE__);
            modem_notify_event(MDM_EVT_NOTIFY_RESET_OFF);
        }
    }
    gpio_irq_cbp_rst_ind();
    oem_gpio_irq_unmask(GPIO_VIATEL_MDM_RST_IND);
    return IRQ_HANDLED;
}

static irqreturn_t modem_power_indication_irq(int irq, void *data)
{
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_IND)){
        oem_gpio_set_irq_type(GPIO_VIATEL_MDM_PWR_IND, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING);
        if(oem_gpio_get_value(GPIO_VIATEL_MDM_PWR_IND)){
            modem_notify_event(MDM_EVT_NOTIFY_POWER_ON);
        }else{
            modem_notify_event(MDM_EVT_NOTIFY_POWER_OFF);
        }
    }
    
    return IRQ_HANDLED;
}

//enable if support 4 pin sync in userspace
#if 0 //defined(CONFIG_VIATELECOM_SYNC_CBP)
static int modem_userspace_notifier(int msg, void *data)
{
     int ret = 0;
     int wake, sleep;
     char *hd = (char *)data;

     if(!hd) {
        printk(KERN_ERR "%s:error sync user\n", __FUNCTION__);
        return -ENODEV;
     }

     if(!strncmp(hd, USB_RX_HD_NAME, ASC_NAME_LEN)){
        wake  = ASC_USER_USB_WAKE;
        sleep = ASC_USER_USB_SLEEP;
     }else if(!strncmp(hd, UART_RX_HD_NAME, ASC_NAME_LEN)){
        wake  = ASC_USER_UART_WAKE;
        sleep = ASC_USER_UART_SLEEP;
     }else if(!strncmp(hd, SDIO_RX_HD_NAME, ASC_NAME_LEN)){
        wake  = ASC_USER_SDIO_WAKE;
        sleep = ASC_USER_SDIO_SLEEP;
     }else{
        return -ENODEV;        
     }

     if(!atomic_read(&vmdata->count)){
        return 0;
     }

     switch(msg){
         case ASC_NTF_RX_PREPARE:
            modem_signal_user(wake);
            break;

        case ASC_NTF_RX_POST:
            modem_signal_user(sleep);
            break;

         default:
            printk("%s unknow message %d\n", __FUNCTION__, msg);
    }

    return ret;
}

static int modem_sync_init(void)
{
    int ret = 0;
    struct asc_infor user;
    struct asc_config cfg;

    /*Registe the cbp tx handle*/
    if(GPIO_OEM_VALID(GPIO_VIATEL_AP_WAKE_MDM) && GPIO_OEM_VALID(GPIO_VIATEL_MDM_RDY)){
        memset(&cfg, 0, sizeof(struct asc_config));
        strncpy(cfg.name, CBP_TX_HD_NAME, ASC_NAME_LEN);
        cfg.gpio_wake = GPIO_VIATEL_AP_WAKE_MDM;
        cfg.gpio_ready = GPIO_VIATEL_MDM_RDY;
        cfg.polar = 1;
        ret = asc_tx_register_handle(&cfg);
        if(ret < 0){
            printk("%s: fail to regist tx handle %s\n", __FUNCTION__, CBP_TX_HD_NAME);
            goto end_sync_init;
        }
    }

    /*Registe the usb rx handle*/
    if(GPIO_OEM_VALID(GPIO_VIATEL_USB_MDM_WAKE_AP) && GPIO_OEM_VALID(GPIO_VIATEL_USB_AP_RDY)){
        memset(&cfg, 0, sizeof(struct asc_config));
        strncpy(cfg.name, USB_RX_HD_NAME, ASC_NAME_LEN);
        cfg.gpio_wake = GPIO_VIATEL_USB_MDM_WAKE_AP;
        cfg.gpio_ready = GPIO_VIATEL_USB_AP_RDY;
        cfg.polar = 1;
        ret = asc_rx_register_handle(&cfg);
        if(ret < 0){
            printk("%s: fail to regist rx handle %s\n", __FUNCTION__, USB_RX_HD_NAME);
            goto end_sync_init;
        }
        memset(&user, 0, sizeof(struct asc_infor));
        user.notifier = modem_userspace_notifier,
        user.data = USB_RX_HD_NAME,
        snprintf(user.name, ASC_NAME_LEN, USB_RX_USER_NAME);
        ret = asc_rx_add_user(USB_RX_HD_NAME, &user);
        if(ret < 0){
            printk("%s: fail to regist rx user %s\n", __FUNCTION__, USB_RX_USER_NAME);
            goto end_sync_init;
        }
    }

    /*Registe the uart rx handle*/
    if(GPIO_OEM_VALID(GPIO_VIATEL_UART_MDM_WAKE_AP) && GPIO_OEM_VALID(GPIO_VIATEL_UART_AP_RDY)){
        memset(&cfg, 0, sizeof(struct asc_config));
        strncpy(cfg.name, UART_RX_HD_NAME, ASC_NAME_LEN);
        cfg.gpio_wake = GPIO_VIATEL_UART_MDM_WAKE_AP;
        cfg.gpio_ready = GPIO_VIATEL_UART_AP_RDY;
        cfg.polar = 1;
        ret = asc_rx_register_handle(&cfg);
        if(ret < 0){
            printk("%s: fail to regist rx handle %s\n", __FUNCTION__, UART_RX_HD_NAME);
            goto end_sync_init;
        }

        memset(&user, 0, sizeof(struct asc_infor));
        user.notifier = modem_userspace_notifier,
        user.data = UART_RX_HD_NAME,
        snprintf(user.name, ASC_NAME_LEN, UART_RX_USER_NAME);
        ret = asc_rx_add_user(UART_RX_HD_NAME, &user);
        if(ret < 0){
            printk("%s: fail to regist rx user %s\n", __FUNCTION__, UART_RX_USER_NAME);
            goto end_sync_init;
        }
    }

end_sync_init:
    if(ret){
        printk("%s: error\n", __FUNCTION__);
    }
    return ret;
}

late_initcall(modem_sync_init);
#endif

static struct platform_driver platform_modem_driver = {
	.driver.name = "via_modem",
	.shutdown = modem_shutdown,
};

static struct platform_device platform_modem_device = {
	.name = "via_modem",
};

static int misc_modem_open(struct inode *inode, struct file *filp)
{
    int ret = -ENODEV;
    if(vmdata){
        filp->private_data = vmdata;
        atomic_inc(&vmdata->count);
        ret = 0;
    }

    return ret;
}

static long misc_modem_ioctl(struct file *file, unsigned int
		cmd, unsigned long arg)
{
    void __user *argp = (void __user *) arg;
    int flag,ret=-1;

    printk("[SDIO MODEM] ioctl %d\n", cmd);
    
    switch (cmd) {
        case VMDM_IOCTL_RESET:
            oem_reset_modem();
            break;
        case VMDM_IOCTL_POWER:
            if (copy_from_user(&flag, argp, sizeof(flag)))
                return -EFAULT;
            if (flag < 0 || flag > 1)
                return -EINVAL;
            if(flag){
                oem_power_on_modem();
            }else{
                oem_power_off_modem();
            }
            break;
	case VMDM_IOCTL_CRL:
		if (copy_from_user(&flag, argp, sizeof(flag)))
				return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;
		if(flag){
			ret=modem_on_off_ctrl_chan(1);
		}else{
			ret=modem_on_off_ctrl_chan(0);
		}
		break;
	case VMDM_IOCTL_DIE:
		oem_let_cbp_die();
		break;
	case VMDM_IOCTL_WAKE:
		if (copy_from_user(&flag, argp, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;
		if(flag){
            printk("hold on wakelock.\n");
			wake_lock(&vmdata->wlock);
		}else{
            printk("release wakelock.\n");
			wake_unlock(&vmdata->wlock);
		}
		break;
	case VMDM_IOCTL_IGNORE:
		if (copy_from_user(&flag, argp, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;
		if(flag){
			printk("Warnning: via ignore notifer.\n");
			via_ignore_notifier = 1;
		}else{
			printk("Warnning: via receive notifer.\n");
			via_ignore_notifier = 0;
		}
		break;
    default:
        break;
			
    }

    return 0;
}

#if CONFIG_COMPAT
static long misc_modem_compat_ioctl( struct file *filp, unsigned int cmd, unsigned long arg)
{
    if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
        printk("[SDIO MODEM]!filp->f_op || !filp->f_op->unlocked_ioctl)\n");
        return -ENOTTY;
    }
    printk("[SDIO MODEM] compat ioctl %d\n", cmd);
    switch(cmd)
    {
        default:
        {
            return filp->f_op->unlocked_ioctl(filp, cmd,
               (unsigned long)compat_ptr(arg));
        }
    }
}
#endif

static int misc_modem_release(struct inode *inode, struct file *filp)
{
    struct viatel_modem_data *d = (struct viatel_modem_data *)(filp->private_data);

    if(atomic_read(&vmdata->count) > 0){
        atomic_dec(&vmdata->count);
    }
    return fasync_helper(-1, filp, 0, &d->fasync);
}

static int misc_modem_fasync(int fd, struct file *filp, int on)
{
    struct viatel_modem_data *d = (struct viatel_modem_data *)(filp->private_data);

    return fasync_helper(fd, filp, on, &d->fasync);
}

static const struct file_operations misc_modem_fops = {
	.owner = THIS_MODULE,
	.open = misc_modem_open,
	.unlocked_ioctl = misc_modem_ioctl,
#if CONFIG_COMPAT    
	.compat_ioctl = &misc_modem_compat_ioctl,
#endif
	.release = misc_modem_release,
	.fasync	= misc_modem_fasync,
};

static struct miscdevice misc_modem_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "vmodem",
	.fops = &misc_modem_fops,
};

static int modem_data_init(struct viatel_modem_data *d)
{
    int ret = 0;
    d->modem_kobj = viatel_kobject_add("modem");
    if(!d->modem_kobj){
        ret = -ENOMEM;
        goto end;
    }
    d->ntf_flags = 0;
    RAW_INIT_NOTIFIER_HEAD(&d->ntf);
    wake_lock_init(&d->wlock, WAKE_LOCK_SUSPEND, "cbp_rst");
    INIT_WORK(&d->work, modem_notify_task);
    d->rst_ntf.notifier_call = modem_reset_notify_misc;
    d->pwr_ntf.notifier_call = modem_power_notify_misc;
	d->err_ntf.notifier_call = modem_err_notify_misc;
    atomic_set(&d->count, 0);
end:
    return ret;
}

static int __init modem_init(void)
{
    int ret = 0;

    vmdata = kzalloc(sizeof(struct viatel_modem_data), GFP_KERNEL);
    if(!vmdata){
        ret = -ENOMEM;
        printk("No memory to alloc vmdata");
        goto err_create_vmdata;
    }

    ret = modem_data_init(vmdata);
    if(ret < 0){
        printk("Fail to init modem data\n");
        goto err_init_modem_data;
    }

    ret = platform_device_register(&platform_modem_device);
    if (ret) {
        printk("platform_device_register failed\n");
        goto err_platform_device_register;
    }
    ret = platform_driver_register(&platform_modem_driver);
    if (ret) {
        printk("platform_driver_register failed\n");
        goto err_platform_driver_register;
    }

    ret = misc_register(&misc_modem_device);
    if(ret < 0){
        printk("misc regiser via modem failed\n");
        goto err_misc_device_register;
    }

    //make the default ETS output through USB
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_ETS_SEL)){
        oem_gpio_direction_output(GPIO_VIATEL_MDM_ETS_SEL, 1);
    }

    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_IND)){
        oem_gpio_irq_mask(GPIO_VIATEL_MDM_PWR_IND);
        oem_gpio_direction_input_for_irq(GPIO_VIATEL_MDM_PWR_IND);
        oem_gpio_set_irq_type(GPIO_VIATEL_MDM_PWR_IND, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING);
        ret = oem_gpio_request_irq(GPIO_VIATEL_MDM_PWR_IND, modem_power_indication_irq, \
                     IRQF_SHARED | IRQF_NO_SUSPEND | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, \
                     "mdm_power_ind", vmdata);
        oem_gpio_irq_unmask(GPIO_VIATEL_MDM_PWR_IND);
        if (ret < 0) {
            printk("fail to request mdm_power_ind irq\n");
        }
        modem_register_notifier(&vmdata->pwr_ntf);
    }

    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST_IND)){
        oem_gpio_irq_mask(GPIO_VIATEL_MDM_RST_IND);
        oem_gpio_direction_input_for_irq(GPIO_VIATEL_MDM_RST_IND);
        oem_gpio_set_irq_type(GPIO_VIATEL_MDM_RST_IND, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING);
        ret = oem_gpio_request_irq(GPIO_VIATEL_MDM_RST_IND, modem_reset_indication_irq, \
                     IRQF_SHARED | IRQF_NO_SUSPEND | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, \
                     "mdm_reset_ind", vmdata);
        oem_gpio_irq_unmask(GPIO_VIATEL_MDM_RST_IND);
        if (ret < 0) {
            printk("fail to request mdm_rst_ind irq\n");
        }
        modem_register_notifier(&vmdata->rst_ntf);
    }

    if(GPIO_OEM_VALID(GPIO_VIATEL_CRASH_CBP)){
		printk("%s %d GPIO_VIATEL_CRASH_CBP",__func__,__LINE__);
		oem_gpio_direction_output(GPIO_VIATEL_CRASH_CBP, 1);
    }
	modem_register_notifier(&vmdata->err_ntf);
    //oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 0);
    //oem_gpio_direction_output(GPIO_VIATEL_MDM_PWR_EN, 1);
    ret = sysfs_create_group(vmdata->modem_kobj, &g_attr_group);

    if(ret){
        printk("sysfs_create_group failed\n");
        goto err_sysfs_create_group; 
    }

    return 0;
err_sysfs_create_group:
    misc_deregister(&misc_modem_device);
err_misc_device_register:
    platform_driver_unregister(&platform_modem_driver);
err_platform_driver_register:
	platform_device_unregister(&platform_modem_device);
err_platform_device_register:
err_init_modem_data:
    kfree(vmdata);
    vmdata = NULL;
err_create_vmdata:
    return ret;
}

static void  __exit modem_exit(void)
{
    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_PWR_IND)){
		modem_unregister_notifier(&vmdata->pwr_ntf);
    }

    if(GPIO_OEM_VALID(GPIO_VIATEL_MDM_RST_IND)){
		modem_unregister_notifier(&vmdata->pwr_ntf);
    }

	modem_unregister_notifier(&vmdata->err_ntf);
	
    if(vmdata)
        wake_lock_destroy(&vmdata->wlock);
}

late_initcall_sync(modem_init);
module_exit(modem_exit);
