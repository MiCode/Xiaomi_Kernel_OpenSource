#ifndef __IR_PWM_H__
#define __IR_PWM_H__

#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/completion.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>


#define LCT_DEBUG_MSG(fmt, args...)  printk(KERN_DEBUG "[IR-PWM]: %s: " fmt, __func__, ##args);
#define LCT_INFO_MSG(fmt, args...)  printk(KERN_INFO "[IR-PWM]: %s: " fmt, __func__, ##args);
#define LCT_ERR_MSG(fmt, args...)  printk(KERN_ERR "[IR-PWM]: %s: " fmt, __func__, ##args);


#define DEVICE_NAME "lct-ir-pwm"

#define PWM_ACTIVE "active"
#define PWM_SLEEP "sleep"
#define GPIO_CLOCK_NAME "gp2_clk"

#define PWM_FREQ 38400
#define BUF_MAX_SIZE 4000


struct pwm_ir_packet {
    struct completion done;
    struct hrtimer timer;
    unsigned int *buffer;
    unsigned int length;
    unsigned int next;
};

struct irled_pwm_dev {
    /* pwm source clock */
    struct clk *s_clk;
    struct mutex write_mutex;

    /* customized dts feature for irtx calibration, qualcomm only */
    bool use_cal_mode;
    unsigned int g_cal_enable_time_with_us;
    unsigned int g_cal_disable_time_with_us;

    /* customized dts feature for vdd config */
    bool vdd_use_pmic;
    unsigned int load_with_uA;
    unsigned int voltage_with_uV;
    struct regulator *vdd;
    int power_enable_gpio;
};

/*
 * Note: DTS string define.
 */

/* for vdd config */
#define DTS_VDD_USE_PMIC_STR "lct,vdd_use_pmic"
#define DTS_LOAD_WITH_UA_STR "lct,load_with_uA"
#define DTS_VOLTAGE_WITH_UV_STR "lct,voltage_with_uV"

/* for irtx calibration */
#define DTS_USE_CAL_MODE_STR "lct,use_cal_mode"
#define DTS_CALIBRATION_ENABLE_STR "lct,cal_enable_time_with_us"
#define DTS_CALIBRATION_DISABLE_STR "lct,cal_disable_time_with_us"

#endif
