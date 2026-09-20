/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2022 TsingTeng MicroSystem Co., Ltd.
 */

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
#include <linux/io.h>
#include <linux/clk.h>
#include "debuger/tms_debuger.h"

/*********** PART1: Driver Version Define Area ***********/
#define MAJOR_VERSION       (1)
#define MINOR_VERSION       (2)
#define MAINTENANCE_VERSION (3)
#define DRIVER_VERSION      (MAJOR_VERSION << 16) + \
                            (MINOR_VERSION << 8) + \
                            (MAINTENANCE_VERSION)

/*********** PART2: Define Area ***********/
#ifdef TMS_MOUDLE
#undef TMS_MOUDLE
#define TMS_MOUDLE                "Common"
#endif
#define DEVICES_CLASS_NAME        "tms"
#define OFF                       0    /* Device power off */
#define ON                        1    /* Device power on */
#define SUCCESS                   0
#define ERROR                     1
#define PAGESIZE                  512
#define I2C_POLL_WAIT_TIME        2000  /* 2ms */
#define WAIT_TIME_NONE            0
#define WAIT_TIME_500US           500
#define WAIT_TIME_1000US          1000
#define WAIT_TIME_5000US          5000
#define WAIT_TIME_10000US         10000
#define WAIT_TIME_20000US         20000
#define GPIO_SET_WAIT_TIME_US     10000
#define GPIO_VEN_SET_WAIT_TIME_US 20000

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#define DECLARE_PROC_OPS(ops, open_func, read_func, write_func, llseek_func, release_func) \
static const struct proc_ops ops = { \
    .proc_open    = open_func,            \
    .proc_read    = read_func,            \
    .proc_write   = write_func,           \
    .proc_lseek   = llseek_func,          \
    .proc_release = release_func,         \
}
#else
#define DECLARE_PROC_OPS(ops, open_func, read_func, write_func, llseek_func, release_func) \
static const struct file_operations ops = { \
    .open    = open_func,                        \
    .read    = read_func,                        \
    .write   = write_func,                       \
    .llseek  = llseek_func,                      \
    .release = release_func,                     \
    .owner   = THIS_MODULE,                      \
}
#endif
/*********** PART3: Struct Area ***********/
struct hw_resource {
    unsigned int        irq_gpio;
    unsigned int        rst_gpio;
    unsigned int        ven_gpio;
    unsigned int        download_gpio; /* nfc fw download control */
    uint32_t            ven_flag;      /* nfc ven setting flag */
    uint32_t            download_flag; /* nfc download setting flag */
    uint32_t            rst_flag;      /* ese reset setting flag */
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

struct tms_feature {
    bool    dl_support : 1; /* DownLoad pin is supported or not */
    bool    rf_clk_enable_support : 1; /* rf clk control is supported or not, unisoc rf clk need to be controlled */
    bool    indept_se_support : 1; /* Independent ese support feature*/
};

struct tms_info {
    bool                        ven_enable; /* store VEN state */
    int                         dev_count;
    char                        *vendor;
    struct class                *class;
    struct hw_resource          hw_res;
    struct proc_dir_entry       *prEntry;
    struct tms_feature          feature;
    int (*registe_device)       (struct dev_register *dev, void *data);
    void (*unregiste_device)    (struct dev_register *dev);
    void (*set_gpio)            (unsigned int gpio, bool state,
                                 unsigned long predelay,
                                 unsigned long postdelay);
};

/*********** PART4: Function or variables for other files ***********/
struct tms_info *tms_common_data_binding(void);
int nfc_driver_init(void);
void nfc_driver_exit(void);
int ese_driver_init(void);
void ese_driver_exit(void);
int tms_guide_init(void);
void tms_guide_exit(void);
#endif /* _TMS_COMMON_H_ */
