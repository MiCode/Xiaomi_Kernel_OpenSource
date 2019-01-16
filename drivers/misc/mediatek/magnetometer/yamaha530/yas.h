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

#ifndef __YAS_H__
#define __YAS_H__

#include "yas_cfg.h"

#define YAS_VERSION                        "4.1.0"
#define __LINUX_KERNEL_DRIVER__


/* -------------------------------------------------------------------------- */
/*  Typedef definition                                                        */
/* -------------------------------------------------------------------------- */

#if defined(__LINUX_KERNEL_DRIVER__)
#include <linux/types.h>
#elif defined(__ANDROID__)
#include <stdint.h>
#else
typedef signed char         int8_t;
typedef unsigned char       uint8_t;
typedef signed short        int16_t;
typedef unsigned short      uint16_t;
typedef signed int          int32_t;
typedef unsigned int        uint32_t;
#endif

/* -------------------------------------------------------------------------- */
/*  Macro definition                                                          */
/* -------------------------------------------------------------------------- */

/* Debugging */
#define DEBUG                               (0)

#define MEDIATEK_CODE

#if DEBUG
#ifdef __LINUX_KERNEL_DRIVER__
#include <linux/kernel.h>
#define YLOGD(args) (printk args )
#define YLOGI(args) (printk args )
#define YLOGE(args) (printk args )
#define YLOGW(args) (printk args )
#elif defined __ANDROID__
#include <cutils/log.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "yas"
#define YLOGD(args) (LOGD args )
#define YLOGI(args) (LOGI args )
#define YLOGE(args) (LOGE args )
#define YLOGW(args) (LOGW args )
#else /* __ANDROID__ */
#include <stdio.h>
#define YLOGD(args) (printf args )
#define YLOGI(args) (printf args )
#define YLOGE(args) (printf args )
#define YLOGW(args) (printf args )
#endif /* __ANDROID__ */
#else /* DEBUG */
#define YLOGD(args) 
#define YLOGI(args) 
#define YLOGW(args) 
#define YLOGE(args) 
#endif /* DEBUG */

#define YAS_REPORT_DATA                     (0x01)
#define YAS_REPORT_CALIB                    (0x02)
#define YAS_REPORT_OVERFLOW_OCCURED         (0x04)
#define YAS_REPORT_HARD_OFFSET_CHANGED      (0x08)
#define YAS_REPORT_CALIB_OFFSET_CHANGED     (0x10)

#define YAS_HARD_OFFSET_UNKNOWN             (0x7f)
#define YAS_CALIB_OFFSET_UNKNOWN            (0x7fffffff)

#define YAS_NO_ERROR                        (0)
#define YAS_ERROR_ARG                       (-1)
#define YAS_ERROR_NOT_INITIALIZED           (-2)
#define YAS_ERROR_BUSY                      (-3)
#define YAS_ERROR_I2C                       (-4)
#define YAS_ERROR_CHIP_ID                   (-5)
#define YAS_ERROR_NOT_ACTIVE                (-6)
#define YAS_ERROR_RESTARTSYS                (-7)
#define YAS_ERROR_HARDOFFSET_NOT_WRITTEN    (-8)
#define YAS_ERROR_ERROR                     (-128)

#ifndef NULL
#define NULL ((void*)(0))
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
#ifndef ABS
#define ABS(a) ((a) > 0 ? (a) : -(a))
#endif
#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

/* -------------------------------------------------------------------------- */
/*  Structure definition                                                      */
/* -------------------------------------------------------------------------- */

struct yas_mag_filter {
    int len;
    int noise[3];
    int threshold; /* nT */
};
struct yas_vector {
    int32_t v[3];
};
struct yas_matrix {
    int32_t matrix[9];
};
struct yas_acc_data {
    struct yas_vector xyz;
    struct yas_vector raw;
};
struct yas_mag_data {
    struct yas_vector xyz; /* without offset, filtered */
    struct yas_vector raw; /* with offset, not filtered */
    struct yas_vector xy1y2;
    int16_t temperature;
};

struct yas_mag_offset {
    int8_t hard_offset[3];
    struct yas_vector calib_offset;
};
struct yas_mag_status {
    struct yas_mag_offset offset;
    int accuracy;
};
struct yas_offset {
    struct yas_mag_status mag[YAS_MAGCALIB_SHAPE_NUM];
};

struct yas_mag_driver_callback {
    int (*lock)(void);
    int (*unlock)(void);
    int (*i2c_open)(void);
    int (*i2c_close)(void);
#if YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS529
    int (*i2c_write)(uint8_t slave, const uint8_t *buf, int len);
    int (*i2c_read)(uint8_t slave, uint8_t *buf, int len);
#else
    int (*i2c_write)(uint8_t slave, uint8_t addr, const uint8_t *buf, int len);
    int (*i2c_read)(uint8_t slave, uint8_t addr, uint8_t *buf, int len);
#endif
    void (*msleep)(int msec);
    void (*current_time)(int32_t *sec, int32_t *msec);
};

struct yas_mag_driver {
    int (*init)(void);
    int (*term)(void);
    int (*get_delay)(void);
    int (*set_delay)(int msec);
    int (*get_offset)(struct yas_mag_offset *offset);
    int (*set_offset)(struct yas_mag_offset *offset);
    int (*get_enable)(void);
    int (*set_enable)(int enable);
    int (*get_filter)(struct yas_mag_filter *filter);
    int (*set_filter)(struct yas_mag_filter *filter);
    int (*get_filter_enable)(void);
    int (*set_filter_enable)(int enable);
    int (*get_position)(void);
    int (*set_position)(int position);
#if YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS529
    int (*read_reg)(uint8_t *buf, int len);
    int (*write_reg)(const uint8_t *buf, int len);
#else
    int (*read_reg)(uint8_t addr, uint8_t *buf, int len);
    int (*write_reg)(uint8_t addr, const uint8_t *buf, int len);
#endif
    int (*measure)(struct yas_mag_data *data, int *time_delay_ms);
    struct yas_mag_driver_callback callback;
};

struct yas_mag_calibration_result {
    int32_t spread;
    int32_t variation;
    int32_t radius;
    int8_t axis;
    int8_t level;
    int8_t accuracy;
};

struct yas_mag_calibration_threshold {
    int32_t spread;
    int32_t variation[3];
};

struct yas_mag_calibration_callback {
    int (*lock)(void);
    int (*unlock)(void);
};

struct yas_mag_calibration {
    int (*init)(void);
    int (*term)(void);
    int (*update)(struct yas_vector *mag,
            struct yas_mag_calibration_result *result);
    int (*get_accuracy)(void);
    int (*set_accuracy)(int accuracy);
    int (*get_offset)(struct yas_vector *offset);
    int (*set_offset)(struct yas_vector *offset);
    int (*get_shape)(void);
    int (*set_shape)(int shape);
    int (*get_threshold)(struct yas_mag_calibration_threshold *threshold);
    int (*set_threshold)(struct yas_mag_calibration_threshold *threshold);
    struct yas_mag_calibration_callback callback;
};

struct yas_acc_filter {
    int threshold; /* um/s^2 */
};

struct yas_acc_driver_callback {
    int (*lock)(void);
    int (*unlock)(void);
    int (*i2c_open)(void);
    int (*i2c_close)(void);
    int (*i2c_write)(uint8_t slave, uint8_t adr, const uint8_t *buf, int len);
    int (*i2c_read) (uint8_t slave, uint8_t adr, uint8_t *buf, int len);
    void (*msleep)(int msec);
};

struct yas_acc_driver {
    int (*init)(void);
    int (*term)(void);
    int (*get_delay)(void);
    int (*set_delay)(int delay);
    int (*get_offset)(struct yas_vector *offset);
    int (*set_offset)(struct yas_vector *offset);
    int (*get_enable)(void);
    int (*set_enable)(int enable);
    int (*get_filter)(struct yas_acc_filter *filter);
    int (*set_filter)(struct yas_acc_filter *filter);
    int (*get_filter_enable)(void);
    int (*set_filter_enable)(int enable);
    int (*get_position)(void);
    int (*set_position)(int position);
    int (*measure)(struct yas_acc_data *data);
#if DEBUG
    int (*get_register)(uint8_t adr, uint8_t *val);
#endif
    struct yas_acc_driver_callback callback;
};

struct yas_acc_calibration_threshold {
    int32_t variation;
};

struct yas_acc_calibration_callback {
    int (*lock)(void);
    int (*unlock)(void);
};

struct yas_acc_calibration {
    int (*init)(void);
    int (*term)(void);
    int (*update)(struct yas_vector *acc);
    int (*get_offset)(struct yas_vector *offset);
    int (*get_threshold)(struct yas_acc_calibration_threshold *threshold);
    int (*set_threshold)(struct yas_acc_calibration_threshold *threshold);
    struct yas_acc_calibration_callback callback;
};

struct yas_utility {
    int (*get_rotation_matrix)(struct yas_vector *acc, struct yas_vector *mag,
            struct yas_matrix *matrix);
    int (*get_euler)(struct yas_matrix *matrix, struct yas_vector *euler);
};

/* -------------------------------------------------------------------------- */
/*  Global function definition                                                */
/* -------------------------------------------------------------------------- */

extern  int yas_mag_driver_init(struct yas_mag_driver *f);
extern  int is_valid_calib_offset(const int32_t *p);
int yas_mag_calibration_init(struct yas_mag_calibration *f);
int yas_acc_driver_init(struct yas_acc_driver *f);
int yas_acc_calibration_init(struct yas_acc_calibration *f);
int yas_utility_init(struct yas_utility *f);

#endif /* __YAS_H__ */
