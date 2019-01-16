/*yamaha529.h - YAMAHA304 compass driver
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * Definitions for yamaha529 compass chip.
 */


#ifndef _YAMAHA529_H_
#define _YAMAHA529_H_

#define YAMAHA304_I2C_ADDRESS 			0x5c  //new Addr=0x0E(Low), old Addr=0x0F(High)

#define YAS529_DEFAULT_THRESHOLD            (1)
#define YAS529_DEFAULT_DISTORTION           (15)
#define YAS529_DEFAULT_SHAPE                (0)

#define __LINUX_KERNEL_DRIVER__

#ifndef STATIC
#define STATIC static
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE (!(0))
#endif

#ifndef NELEMS
#define NELEMS(a) ((int)(sizeof(a)/sizeof(a[0])))
#endif

#ifdef __LINUX_KERNEL_DRIVER__
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/slab.h>

/*
#define YLOGD(...) 
#define YLOGI(...) 
#define YLOGW(...) printk(KERN_WARNING __VA_ARGS__)
#define YLOGE(...) printk(KERN_ERR __VA_ARGS__)
*/

#define MSE_TAG                  "MSENSOR"
#define MSE_FUN(f)               printk(KERN_INFO MSE_TAG" %s\r\n", __FUNCTION__)
#define MSE_ERR(fmt, args...)    printk(KERN_ERR MSE_TAG" %s %d : \r\n"fmt, __FUNCTION__, __LINE__, ##args)
#define MSE_LOG(fmt, args...)    printk(KERN_INFO MSE_TAG fmt, ##args)
#define MSE_VER(fmt, args...)   ((void)0)

#else
#include <stdint.h>
#define YLOGD(...) 
#define YLOGI(...) 
#define YLOGW(...) 
#define YLOGE(...) 
#endif

#ifndef __UTIMER_H__
#define __UTIMER_H__

struct utimeval {
    int32_t tv_sec;
    int32_t tv_msec;
};

struct utimer {
    struct utimeval prev_time;
    struct utimeval total_time;
    struct utimeval delay_ms;
};

STATIC int utimeval_init(struct utimeval *val);
STATIC int utimeval_is_initial(struct utimeval *val);
STATIC int utimeval_is_overflow(struct utimeval *val);
STATIC struct utimeval utimeval_plus(struct utimeval *first, struct utimeval *second);
STATIC struct utimeval utimeval_minus(struct utimeval *first, struct utimeval *second);
STATIC int utimeval_greater_than(struct utimeval *first, struct utimeval *second);
STATIC int utimeval_greater_or_equal(struct utimeval *first, struct utimeval *second);
STATIC int utimeval_greater_than_zero(struct utimeval *val);
STATIC int utimeval_less_than_zero(struct utimeval *val);
STATIC struct utimeval *msec_to_utimeval(struct utimeval *result, uint32_t msec);
STATIC uint32_t utimeval_to_msec(struct utimeval *val);

STATIC struct utimeval utimer_calc_next_time(struct utimer *ut,
                                             struct utimeval *cur);
STATIC struct utimeval utimer_current_time(void);
STATIC int utimer_is_timeout(struct utimer *ut);
STATIC int utimer_clear_timeout(struct utimer *ut);
STATIC uint32_t utimer_get_delay(struct utimer *ut);
STATIC int utimer_set_delay(struct utimer *ut, uint32_t delay_ms);
STATIC int utimer_update(struct utimer *ut);
STATIC int utimer_update_with_curtime(struct utimer *ut, struct utimeval *cur);
STATIC uint32_t utimer_sleep_time_with_curtime(struct utimer *ut,
                                               struct utimeval *cur);
STATIC int utimer_init(struct utimer *ut, uint32_t delay_ms);
STATIC int utimer_clear(struct utimer *ut);
STATIC void utimer_lib_init(void (*func)(int *sec, int *msec));

#endif

#ifndef __YAS529_CDRIVER_H__
#define __YAS529_CDRIVER_H__

# define YAS529_CDRV_CENTER_X  512
# define YAS529_CDRV_CENTER_Y1 512
# define YAS529_CDRV_CENTER_Y2 512
# define YAS529_CDRV_CENTER_T  256
# define YAS529_CDRV_CENTER_I1 512
# define YAS529_CDRV_CENTER_I2 512
# define YAS529_CDRV_CENTER_I3 512

#define YAS529_CDRV_ROUGHOFFSET_MEASURE_OF_VALUE 33
#define YAS529_CDRV_ROUGHOFFSET_MEASURE_UF_VALUE  0
#define YAS529_CDRV_NORMAL_MEASURE_OF_VALUE 1024
#define YAS529_CDRV_NORMAL_MEASURE_UF_VALUE    1

#define MS3CDRV_CMD_MEASURE_ROUGHOFFSET 0x1
#define MS3CDRV_CMD_MEASURE_XY1Y2T      0x2

#define MS3CDRV_RDSEL_MEASURE     0xc0
#define MS3CDRV_RDSEL_CALREGISTER 0xc8

#define MS3CDRV_WAIT_MEASURE_ROUGHOFFSET  2 /*  1.5[ms] */
#define MS3CDRV_WAIT_MEASURE_XY1Y2T      13 /* 12.3[ms] */

#define MS3CDRV_I2C_SLAVE_ADDRESS 0x2e
#define MS3CDRV_GSENSOR_INITIALIZED     (0x01)
#define MS3CDRV_MSENSOR_INITIALIZED     (0x02)

#define YAS529_CDRV_NO_ERROR 0
#define YAS529_CDRV_ERR_ARG (-1)
#define YAS529_CDRV_ERR_NOT_INITIALIZED (-3)
#define YAS529_CDRV_ERR_BUSY (-4)
#define YAS529_CDRV_ERR_I2CCTRL (-5)
#define YAS529_CDRV_ERR_ROUGHOFFSET_NOT_WRITTEN (-126)
#define YAS529_CDRV_ERROR (-127)

#define YAS529_CDRV_MEASURE_X_OFUF  0x1
#define YAS529_CDRV_MEASURE_Y1_OFUF 0x2
#define YAS529_CDRV_MEASURE_Y2_OFUF 0x4

#define YAS529_RDSEL_DEVICEID     0xd0
#define YAS529_DEVICE_ID		0x40
struct yas529_machdep_func {
    int (*i2c_open)(void);
    int (*i2c_close)(void);
    int (*i2c_write)(uint8_t slave, const uint8_t *buf, int len);
    int (*i2c_read)(uint8_t slave, uint8_t *buf, int len);
    void (*msleep)(int ms);
};

STATIC int yas529_cdrv_actuate_initcoil(void);
STATIC int yas529_cdrv_set_rough_offset(const uint8_t *rough_offset);
STATIC int yas529_cdrv_recalc_fine_offset(int32_t *prev_fine_offset,
                                          int32_t *new_fine_offset,
                                          uint8_t *prev_rough_offset,
                                          uint8_t *new_rough_offset);
STATIC int yas529_cdrv_set_transformatiom_matrix(const int8_t *transform);
STATIC int yas529_cdrv_measure_rough_offset(uint8_t *rough_offset);
STATIC int yas529_cdrv_measure(int32_t *msens, int32_t *raw, int16_t *t);
STATIC int yas529_cdrv_init(const int8_t *transform,
                            struct yas529_machdep_func *func);
STATIC int yas529_cdrv_term(void);

#endif

#ifndef __YAS529_DRIVER_H__
#define __YAS529_DRIVER_H__

#define YAS529_NO_ERROR                 (0)
#define YAS529_ERROR_ARG                (YAS529_CDRV_ERR_ARG)
#define YAS529_ERROR_NOT_INITIALIZED    (YAS529_CDRV_ERR_NOT_INITIALIZED)
#define YAS529_ERROR_BUSY               (YAS529_CDRV_ERR_BUSY)
#define YAS529_ERROR_I2C                (YAS529_CDRV_ERR_I2CCTRL)
#define YAS529_ERROR_NOT_ACTIVE         (-124)
#define YAS529_ERROR_ROUGHOFFSET_NOT_WRITTEN (YAS529_CDRV_ERR_ROUGHOFFSET_NOT_WRITTEN)
#define YAS529_ERROR_ERROR              (YAS529_CDRV_ERROR)
#define YAS529_ERROR_RESTARTSYS         (-512)

#define YAS529_IOC_GET_DRIVER_STATE     (1)
#define YAS529_IOC_SET_DRIVER_STATE     (2)

#define YAS529_REPORT_DATA                  (0x01)
#define YAS529_REPORT_CALIB                 (0x02)
#define YAS529_REPORT_OVERFLOW_OCCURED      (0x04)
#define YAS529_REPORT_ROUGH_OFFSET_CHANGED  (0x08)
#define YAS529_REPORT_FINE_OFFSET_CHANGED   (0x10)

struct yas529_driver_state {
    int32_t fine_offset[3];
    uint8_t rough_offset[3];
    int accuracy;
};

struct geomagnetic_hwdep_driver {
    int (*init)(void);
    int (*term)(void);
    int (*get_enable)(void);
    int (*set_enable)(int enable);
    int (*get_filter_enable)(void);
    int (*set_filter_enable)(int enable);
    int (*get_filter_len)(void);
    int (*set_filter_len)(int len);
    int (*get_delay)(void);
    int (*set_delay)(int delay);
    int (*get_position)(void);
    int (*set_position)(int accuracy);
    int (*measure)(int32_t *magnetic, int32_t *raw, int32_t *accuracy,
                   uint32_t *time_delay_ms);
    int (*ioctl)(unsigned int cmd, unsigned long args);

    struct geomagnetic_hwdep_callback {
        int (*lock)(void);
        int (*unlock)(void);
        int (*i2c_open)(void);
        int (*i2c_close)(void);
        int (*i2c_write)(uint8_t slave, const uint8_t *buf, int len);
        int (*i2c_read)(uint8_t slave, uint8_t *buf, int len);
        void (*msleep)(int ms);
        void (*current_time)(int32_t *sec, int32_t *msec);
    } callback;
};

STATIC int geomagnetic_driver_init(struct geomagnetic_hwdep_driver *hwdep_driver);

#endif

/*
 * Copyright (c) 2010 Yamaha Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef _YAS529_LINUX_H_
#define _YAS529_LINUX_H_

#define GEOMAGNETIC_I2C_DEVICE_NAME     "yas529"
#define GEOMAGNETIC_DEVICE_NAME         "yas529"
#define GEOMAGNETIC_INPUT_NAME          "geomagnetic"
#define GEOMAGNETIC_INPUT_RAW_NAME      "geomagnetic_raw"

#endif /* _YAS529_LINUX_H_ */

#endif /* _YAS529_H_ */

