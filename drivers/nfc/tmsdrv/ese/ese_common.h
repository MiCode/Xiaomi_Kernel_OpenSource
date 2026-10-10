/*****************************************************************************************
 * Copyright (c)  2021- 2022  TsingTeng MicroSystem Corp.Ltd.
 * TMS_EDIT
 * File       : ese_common.h
 * Description: Source file for tms ese common
 * Version    : 1.0
 * Date       : 2022/4/11
 * Team       : NFC Middleware
 * Author     : Guoliang Wu
 * --------------------------------- Revision History: ---------------------------------
 *   <version>    <date>          < author >                            <desc>
 *******************************************************************************************/
#ifndef _TMS_ESE_H_
#define _TMS_ESE_H_

/*********** PART0: Head files ***********/
#include <linux/spi/spi.h>

#include "../tms_common.h"
/*********** PART1: Define Area ***********/
#ifdef TMS_MOUDLE
#undef TMS_MOUDLE
#define TMS_MOUDLE               "eSE"
#endif
#define ESE_VERSION              "1.0.221230"
/*********** PART2: Struct Area ***********/
struct ese_info {
    bool                   independent_support; /* irq support feature*/
    bool                   irq_enable;
    bool                   release_read;
    struct spi_device      *client;
    struct device          *spi_dev;            /* Used for spi->dev */
    struct dev_register    dev;
    struct hw_resource     hw_res;
    struct tms_info        *tms;                /* tms common data */
    struct mutex           read_mutex;
    struct mutex           write_mutex;
    spinlock_t             irq_enable_slock;
    wait_queue_head_t      read_wq;

};

/*********** PART3: Function or variables for other files ***********/
struct ese_info *ese_data_alloc(struct device *dev, struct ese_info *ese);
void ese_data_free(struct device *dev, struct ese_info *ese);
int ese_common_info_init(struct ese_info *ese);
void ese_gpio_release(struct ese_info *ese);
struct ese_info *ese_get_data(struct inode *inode);
void ese_power_control(struct ese_info *ese, bool state);
void ese_enable_irq(struct ese_info *ese);
void ese_disable_irq(struct ese_info *ese);
int ese_irq_register(struct ese_info *ese);
#endif /* _TMS_ESE_H_ */
