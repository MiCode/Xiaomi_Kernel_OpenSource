/*****************************************************************************************
 * Copyright (c)  2021- 2022  TsingTeng MicroSystem Corp.Ltd.
 * TMS_EDIT
 * File       : tms_common.h
 * Description: Source file for tms device common
 * Version    : 1.0
 * Date       : 2022/4/11
 * Team       : NFC Middleware
 * Author     : Guoliang Wu
 * --------------------------------- Revision History: ---------------------------------
 *   <version>    <date>          < author >                            <desc>
 *******************************************************************************************/
#ifndef _TMS_COMMON_H_
#define _TMS_COMMON_H_

/*********** PART0: Head files ***********/
#include <linux/version.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/io.h>

/*********** PART1: LOG TAG Declear***********/
#define log_fmt(fmt) "[TMS-%s]%s: " fmt
#define TMS_ERR(a, arg...)\
    pr_err(log_fmt(a), TMS_MOUDLE, __func__, ##arg)

#define TMS_WARN(a, arg...)\
    do{\
        if (tms_debug >= LEVEL_WARN)\
            pr_err(log_fmt(a), TMS_MOUDLE, __func__, ##arg);\
    }while(0)

#define TMS_INFO(a, arg...)\
    do{\
        if (tms_debug >= LEVEL_INFO)\
            pr_err(log_fmt(a), TMS_MOUDLE, __func__, ##arg);\
    }while(0)

#define TMS_DEBUG(a, arg...)\
    do{\
        if (tms_debug >= LEVEL_DEBUG)\
            pr_err(log_fmt(a), TMS_MOUDLE, __func__, ##arg);\
    }while(0)

/*********** PART2: Define Area ***********/
#define TMS_MOUDLE                "Common"
#define DRIVER_VERSION            "1.0.221230"
#define DEVICES_CLASS_NAME        "tms"
#define OFF                       0    /* Device power off */
#define ON                        1    /* Device power on */
#define SUCCESS                   0
#define ERROR                     1
#define PAGESIZE                  512
#define GPIO_SET_WAIT_TIME_US     10000
#define GPIO_VEN_SET_WAIT_TIME_US 20000
/*********** PART3: Struct Area ***********/
struct hw_resource {
    unsigned int    irq_gpio;
    unsigned int    rst_gpio;
    unsigned int    ven_gpio;
    unsigned int    download_gpio; /* nfc fw download control */
    uint32_t        ven_flag;      /* nfc ven setting flag */
    uint32_t        download_flag; /* nfc download setting flag */
    uint32_t        rst_flag;      /* ese reset setting flag */
};

struct dev_register {
    unsigned int                    count;     /* Number of devices */
    const char                      *name;     /* device name */
    dev_t                           devno;     /* request a device number */
    struct device                   *creation;
    struct cdev                     chrdev;    /* Used for char device */
    struct class                    *class;
    const struct file_operations    *fops;
};

struct tms_info {
    bool                        ven_enable; /* store VEN state */
    int                         dev_count;
    char                        *nfc_name;
    struct class                *class;
    struct hw_resource          hw_res;
    struct proc_dir_entry       *prEntry;
    int (*registe_device)       (struct dev_register *dev, void *data);
    void (*unregiste_device)    (struct dev_register *dev);
    void (*set_ven)             (struct hw_resource hw_res, bool state);
    void (*set_download)        (struct hw_resource hw_res, bool state);
    void (*set_reset)           (struct hw_resource hw_res, bool state);

};

typedef enum {
    LEVEL_WARN = 1, /* printk warning debug info */
    LEVEL_INFO,     /* printk basic debug info */
    LEVEL_DEBUG,    /* printk all debug info */
    LEVEL_DUMP,     /* printk buffer info */
} tms_debug_level;

/*********** PART4: Function or variables for other files ***********/
extern unsigned int tms_debug;
struct tms_info *tms_common_data_binding(void);
void tms_buffer_dump(const char *tag, const uint8_t *src, int16_t len);
int tms_common_init(void);
void tms_common_exit(void);
#endif /* _TMS_COMMON_H_ */
