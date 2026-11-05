#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/pm_wakeirq.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/cdev.h>
#include <linux/version.h>


#define MINOR_NUMBER_COUNT 1

#define TT_IOC_MAGIC 'M'
#define TT_POWER_ON            _IOWR(TT_IOC_MAGIC, 0, int)
#define TT_POWER_OFF           _IOWR(TT_IOC_MAGIC, 1, int)

#define TIANTONG_DEV_MODE_MSK (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH) /*664*/

#define XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(_name, _gpio_idx) \
 \
static ssize_t gpio_tiantong_get_##_name(struct device *device, \
        struct device_attribute *attr, char *buf) \
{ \
    return gpio_tiantong_get_general(device, _gpio_idx, buf); \
} \
 \
static ssize_t gpio_tiantong_set_##_name(struct device *device, \
                              struct device_attribute *attr, \
                              const char *buf, size_t count) \
{ \
    return gpio_tiantong_set_general(device, _gpio_idx, buf, count); \
} \
 \
static DEVICE_ATTR(_name, TIANTONG_DEV_MODE_MSK, gpio_tiantong_get_##_name, gpio_tiantong_set_##_name);

#define XIAOMI_TIANTONG_REGULATOR_PWR_DEVICE_ATTR(_name, _regulator_idx) \
static ssize_t reg_tiantong_pwr_read_##_name(struct device *device, \
                              struct device_attribute *attr, char *buf) \
{ \
    return reg_tiantong_read_general(device, _regulator_idx, buf); \
} \
 \
static ssize_t reg_tiantong_pwr_ctl_##_name(struct device *device, \
                              struct device_attribute *attr, \
                              const char *buf, size_t count) \
{ \
    return reg_tiantong_set_general(device, _regulator_idx, buf, count); \
} \
 \
static DEVICE_ATTR(_name, TIANTONG_DEV_MODE_MSK, reg_tiantong_pwr_read_##_name, reg_tiantong_pwr_ctl_##_name);

enum {
    TIANTONG_GPIO_BOOT_MODE0 = 0,
    TIANTONG_GPIO_BOOT_MODE1,
    TIANTONG_GPIO_AP_SLEEP_BP,
    TIANTONG_GPIO_BP_SLEEP_AP,
    TIANTONG_GPIO_RST,
    TIANTONG_GPIO_SIM1_EN,
    TIANTONG_GPIO_SIM2_EN,
    TIANTONG_GPIO_SIM1_VDD_QC_EN,
    TIANTONG_GPIO_SIM1_VDD_TT_EN,
    TIANTONG_GPIO_SIM2_VDD_QC_EN,
    TIANTONG_GPIO_SIM2_VDD_TT_EN,
    TIANTONG_GPIO_PA_BOOST_EN,
    TIANTONG_GPIO_TT_1P8_EN,
    TIANTONG_GPIO_SP4T_EN,
    TIANTONG_GPIO_PMIC_EN,
    TIANTONG_GPIO_PMIC_IRQ,
    TIANTONG_GPIO_MAX,
};

static const char * const tiantong_gpio_names[] = {
    "gpio-boot-mode0",
    "gpio-boot-mode1",
    "gpio-ap-sleep-bp",
    "gpio-bp-sleep-ap",
    "gpio-rst",
    "gpio-sim1-en",
    "gpio-sim2-en",
    "gpio-sim1-vdd-qc-en",
    "gpio-sim1-vdd-tt-en",
    "gpio-sim2-vdd-qc-en",
    "gpio-sim2-vdd-tt-en",
    "gpio-pa-boost-en",
    "gpio-tt-1p8-en",
    "gpio-sp4t-en",
    "gpio-pmic-en",
    "gpio-pmic-irq",
};

enum {
    TIANTONG_REG_PWR_CTL_TT_VDD_SW_1P2 = 0,
    TIANTONG_REG_PWR_CTL_TT_VDD_1P2,
    TIANTONG_REG_PWR_CTL_TT_BBIC_3P4,
    TIANTONG_REG_PWR_CTL_TT_VDD_SIM_1P8,
    TIANTONG_REG_PWR_CTL_TT_VDDIO_SW_1P8,
    TIANTONG_REG_PWR_CTL_TT_AVDD_1P8,
    TIANTONG_REG_PWR_CTL_TT_LNA_2P8,
    TIANTONG_REG_PWR_CTL_TT_VDD_SIM_3P0,
    TIANTONG_REG_PWR_MAX,
};

static const char * const tiantong_regulator_name_array[] = {
    "ttvddsw1p2_vdd",
    "ttvdd1p2_vdd",
    "ttbbic3p4_vdd",
    "ttvddsim1p8_vdd",
    "ttvddiosw1p8_vdd",
    "ttavdd1p8_vdd",
    "ttlna2p8_vdd",
    "ttvddsim3p0_vdd",
};

struct regulator_data {
    int   enable_uv;
    int   disable_uv;
};

static struct regulator_data reg_voltage[] = {
    {1200000, 496000},
    {1200000, 496000},
    {3400000, 1504000},
    {1800000, 1504000},
    {1800000, 1504000},
    {1800000, 1504000},
    {2800000, 1504000},
    {3000000, 1504000},
};

struct gpio_data {
    int irq;
    int gpio_num;
    int gpio_status;
};

struct tiantong_gpio_data {
    struct device *dev;
    int debounce_time;
    int current_irq;

    struct mutex lock;  /* To set/get exported values in sysfs */

    struct delayed_work debounce_work;
    struct gpio_data *data;

    struct regulator *tt_regulator_array[TIANTONG_REG_PWR_MAX];

    /* char dev related data */
    char *driver_name;
    dev_t tiantong_major;
    struct class *tiantong_class;
    struct device *chardev;
    struct cdev cdev;
    struct fasync_struct *async;
};


static ssize_t gpio_tiantong_set_general(struct device *device, int gpio_type, const char *buf, size_t count)
{
    int rc = -1;
    int gpio = 0;
    struct tiantong_gpio_data *tiantong_data;

    tiantong_data = dev_get_drvdata(device);
    gpio = tiantong_data->data[gpio_type].gpio_num;

    mutex_lock(&tiantong_data->lock);
    if (!strncmp(buf, "1", strlen("1"))) {
        if (gpio_is_valid(gpio)) {
            rc = gpio_direction_output(gpio, 1);
            if (rc) {
                dev_err(device, "tiantong %s: fail to set gpio %d !\n", __func__, gpio);
                goto unlock_ret;
            }
        } else {
            dev_err(device, "tiantong %s: unable to get gpio %d!\n", __func__, gpio);
        }
    } else if (!strncmp(buf, "0", strlen("0"))) {
        if (gpio_is_valid(gpio)) {
            rc = gpio_direction_output(gpio, 0);
            if (rc) {
                dev_err(device, "tiantong %s: fail to set gpio %d !\n", __func__, gpio);
                goto unlock_ret;
            }
        } else {
            dev_err(device, "tiantong %s: unable to get gpio %d!\n", __func__, gpio);
        }
    } else {
        rc = -EINVAL;
        dev_err(device, "tiantong %s: invalid input %s!only 0 or 1 is valid.\n", __func__, buf);
    }

unlock_ret:
    mutex_unlock(&tiantong_data->lock);
    dev_info(device, "%s, gpio_type [%d] set, rc [%d], buf = %s\n", __func__, gpio_type, rc, buf);
    return count;
}



static ssize_t gpio_tiantong_get_general(struct device *device, int gpio_type, char *buf)
{
    int value = -1;
    int gpio = 0;
    struct tiantong_gpio_data *tiantong_data;
    tiantong_data = dev_get_drvdata(device);

    mutex_lock(&tiantong_data->lock);
    tiantong_data = dev_get_drvdata(device);
    gpio = tiantong_data->data[gpio_type].gpio_num;

    if (gpio_is_valid(gpio)) {
        value = gpio_get_value(gpio);
    } else {
        dev_err(device, "tiantong %s: unable to get gpio %d!\n", __func__, gpio);
    }
    mutex_unlock(&tiantong_data->lock);

    dev_info(device, "%s, gpio_type [%d] get, value [%d]\n", __func__, gpio_type, value);
    return sysfs_emit(buf, "%d\n", value);
}


XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(boot_mode0, TIANTONG_GPIO_BOOT_MODE0)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(boot_mode1, TIANTONG_GPIO_BOOT_MODE1)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(ap_sleep_bp, TIANTONG_GPIO_AP_SLEEP_BP)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(bp_sleep_ap, TIANTONG_GPIO_BP_SLEEP_AP)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(rst, TIANTONG_GPIO_RST)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(sim1_en, TIANTONG_GPIO_SIM1_EN)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(sim2_en, TIANTONG_GPIO_SIM2_EN)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(sim1_vdd_qc_en, TIANTONG_GPIO_SIM1_VDD_QC_EN)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(sim1_vdd_tt_en, TIANTONG_GPIO_SIM1_VDD_TT_EN)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(sim2_vdd_qc_en, TIANTONG_GPIO_SIM2_VDD_QC_EN)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(sim2_vdd_tt_en, TIANTONG_GPIO_SIM2_VDD_TT_EN)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(pa_boost_en, TIANTONG_GPIO_PA_BOOST_EN)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(tt_1p8_en, TIANTONG_GPIO_TT_1P8_EN)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(sp4t_en, TIANTONG_GPIO_SP4T_EN)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(pmic_en, TIANTONG_GPIO_PMIC_EN)
XIAOMI_GPIO_TIANTONG_DEVICE_ATTR(pmic_irq, TIANTONG_GPIO_PMIC_IRQ)


static ssize_t reg_tiantong_set_general(struct device *device,
            int pwr_ctl_type,
            const char *buf, size_t count)
{
    bool enable = false;
    int ret_val = -1;
    struct regulator * reg = NULL;

    struct tiantong_gpio_data *tiantong_data;
    tiantong_data = dev_get_drvdata(device);

    pr_info("%s enter, pwr_ctl_type %d\n", __func__, pwr_ctl_type);

    if (!strncmp(buf, "1", strlen("1"))) {
        enable = true;
    } else {
        enable = false;
    }

    mutex_lock(&tiantong_data->lock);

    if(pwr_ctl_type >= TIANTONG_REG_PWR_CTL_TT_VDD_SW_1P2 && pwr_ctl_type < TIANTONG_REG_PWR_MAX) {
        reg = tiantong_data->tt_regulator_array[pwr_ctl_type];
    } else {
        dev_info(device, "%s: no valid regulator %d\n", __func__, pwr_ctl_type);
    }

    if(reg) {
        if(enable) {
            if(!regulator_is_enabled(reg)) {
                dev_info(device, "%s: regulator_set_voltage %d\n", __func__, reg_voltage[pwr_ctl_type].enable_uv);
                regulator_set_voltage(reg, reg_voltage[pwr_ctl_type].enable_uv, reg_voltage[pwr_ctl_type].enable_uv);
                ret_val = regulator_enable(reg);
                if (ret_val < 0) {
                    dev_info(device, "%s: Failed to enable power regulator %d\n", __func__, pwr_ctl_type);
                }
            } else {
                pr_info("tiantong %s already enabled\n", tiantong_regulator_name_array[pwr_ctl_type]);
            }
        } else {
            if(regulator_is_enabled(reg)) {
                ret_val = regulator_disable(reg);
                regulator_set_voltage(reg, reg_voltage[pwr_ctl_type].disable_uv, reg_voltage[pwr_ctl_type].disable_uv);
                pr_info("tiantong disable %s ret %d.\n", tiantong_regulator_name_array[pwr_ctl_type], ret_val);
            } else {
                pr_info("tiantong %s already disabled\n", tiantong_regulator_name_array[pwr_ctl_type]);
            }
        }
    } else {
        dev_info(device, "%s, type: %d, enable: %d, reg is NULL\n", __func__, pwr_ctl_type, enable);
    }
    mutex_unlock(&tiantong_data->lock);
    dev_info(device, "%s, type: %d, enable: %d, result: %d\n", __func__, pwr_ctl_type, enable, ret_val);
    return count;
}


static ssize_t reg_tiantong_read_general(struct device *device, int pwr_ctl_type, char *buf)
{
    int value = -1;
    struct regulator * reg = NULL;
    struct tiantong_gpio_data *tiantong_data;
    tiantong_data = dev_get_drvdata(device);

    pr_info("%s enter\n", __func__);

    mutex_lock(&tiantong_data->lock);

    if(pwr_ctl_type >= TIANTONG_REG_PWR_CTL_TT_VDD_SW_1P2 && pwr_ctl_type < TIANTONG_REG_PWR_MAX) {
        reg = tiantong_data->tt_regulator_array[pwr_ctl_type];
    } else {
        dev_info(device, "%s: no valid regulator %d\n", __func__, pwr_ctl_type);
    }

    if (reg) {
        value = regulator_get_voltage(reg);
        if (value < 0) {
            dev_info(device, "%s: Failed to get  valtage for regulator %d, vol = %d\n", __func__, pwr_ctl_type, value);
        }
    }
    scnprintf(buf, PAGE_SIZE, "%d\n", value);

    mutex_unlock(&tiantong_data->lock);
    dev_info(device, "%s, reg_type [%d] get, value [%d], buf = %s\n", __func__, pwr_ctl_type, value, buf);
    return sysfs_emit(buf, "%d\n", value);
}


XIAOMI_TIANTONG_REGULATOR_PWR_DEVICE_ATTR(tt_vdd_sw_1p2, TIANTONG_REG_PWR_CTL_TT_VDD_SW_1P2)
XIAOMI_TIANTONG_REGULATOR_PWR_DEVICE_ATTR(tt_vdd_1p2, TIANTONG_REG_PWR_CTL_TT_VDD_1P2)
XIAOMI_TIANTONG_REGULATOR_PWR_DEVICE_ATTR(tt_bbic_3p4, TIANTONG_REG_PWR_CTL_TT_BBIC_3P4)
XIAOMI_TIANTONG_REGULATOR_PWR_DEVICE_ATTR(tt_vdd_sim_1p8, TIANTONG_REG_PWR_CTL_TT_VDD_SIM_1P8)
XIAOMI_TIANTONG_REGULATOR_PWR_DEVICE_ATTR(tt_vddio_sw_1p8, TIANTONG_REG_PWR_CTL_TT_VDDIO_SW_1P8)
XIAOMI_TIANTONG_REGULATOR_PWR_DEVICE_ATTR(tt_avdd_1p8, TIANTONG_REG_PWR_CTL_TT_AVDD_1P8)
XIAOMI_TIANTONG_REGULATOR_PWR_DEVICE_ATTR(tt_lna_2p8, TIANTONG_REG_PWR_CTL_TT_LNA_2P8)
XIAOMI_TIANTONG_REGULATOR_PWR_DEVICE_ATTR(tt_vdd_sim_3p0, TIANTONG_REG_PWR_CTL_TT_VDD_SIM_3P0)


static struct attribute *attributes[] = {
    &dev_attr_boot_mode0.attr,
    &dev_attr_boot_mode1.attr,
    &dev_attr_ap_sleep_bp.attr,
    &dev_attr_bp_sleep_ap.attr,
    &dev_attr_rst.attr,
    &dev_attr_sim1_en.attr,
    &dev_attr_sim2_en.attr,
    &dev_attr_sim1_vdd_qc_en.attr,
    &dev_attr_sim1_vdd_tt_en.attr,
    &dev_attr_sim2_vdd_qc_en.attr,
    &dev_attr_sim2_vdd_tt_en.attr,
    &dev_attr_pa_boost_en.attr,
    &dev_attr_tt_1p8_en.attr,
    &dev_attr_sp4t_en.attr,
    &dev_attr_pmic_en.attr,
    &dev_attr_pmic_irq.attr,

    &dev_attr_tt_vdd_sw_1p2.attr,
    &dev_attr_tt_vdd_1p2.attr,
    &dev_attr_tt_bbic_3p4.attr,
    &dev_attr_tt_vdd_sim_1p8.attr,
    &dev_attr_tt_vddio_sw_1p8.attr,
    &dev_attr_tt_avdd_1p8.attr,
    &dev_attr_tt_lna_2p8.attr,
    &dev_attr_tt_vdd_sim_3p0.attr,
    NULL
};

static const struct attribute_group attribute_group = {
    .attrs = attributes,
};


#define MAX_MSG_LENGTH 20
static void gpio_debounce_work(struct work_struct *work)
{
    struct tiantong_gpio_data *tiantong_data =
            container_of(work, struct tiantong_gpio_data, debounce_work.work);
    struct device *dev = tiantong_data->dev;
    char status_env[MAX_MSG_LENGTH];
    char *envp[] = { status_env, NULL };

    //only update changed gpio.
    if ( tiantong_data->data[TIANTONG_GPIO_BP_SLEEP_AP].irq == tiantong_data->current_irq ) {
        int gpio_status = gpio_get_value(tiantong_data->data[TIANTONG_GPIO_BP_SLEEP_AP].gpio_num);
        if (gpio_status == tiantong_data->data[TIANTONG_GPIO_BP_SLEEP_AP].gpio_status) {
            snprintf(status_env, MAX_MSG_LENGTH, "STATUS=%d", gpio_status);
            dev_info(dev, "Update testing mode status: %d\n", gpio_status);
            kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
        }
    } else if(tiantong_data->data[TIANTONG_GPIO_PMIC_IRQ].irq == tiantong_data->current_irq) {
        int gpio_status = gpio_get_value(tiantong_data->data[TIANTONG_GPIO_PMIC_IRQ].gpio_num);
        if (gpio_status == tiantong_data->data[TIANTONG_GPIO_PMIC_IRQ].gpio_status) {
            snprintf(status_env, MAX_MSG_LENGTH, "PMIC_STATUS=%d", gpio_status);
            dev_info(dev, "Update PMIC_STATUS status: %d\n", gpio_status);
            kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
        }
    } else {
        dev_err(dev, "unknow irq %d\n", tiantong_data->current_irq);
    }
}

static irqreturn_t gpio_tiantong_threaded_irq_handler(int irq, void *irq_data)
{
    struct tiantong_gpio_data *tiantong_data = irq_data;
    struct device *dev = tiantong_data->dev;

    dev_info(dev, "irq [%d] triggered\n", irq);
    if ( irq == tiantong_data->data[TIANTONG_GPIO_BP_SLEEP_AP].irq ) {
        tiantong_data->data[TIANTONG_GPIO_BP_SLEEP_AP].gpio_status =
            gpio_get_value(tiantong_data->data[TIANTONG_GPIO_BP_SLEEP_AP].gpio_num);
    } else if ( irq == tiantong_data->data[TIANTONG_GPIO_PMIC_IRQ].irq ) {
        tiantong_data->data[TIANTONG_GPIO_PMIC_IRQ].gpio_status =
            gpio_get_value(tiantong_data->data[TIANTONG_GPIO_PMIC_IRQ].gpio_num);
    } else {
        dev_err(dev, "unknow irq [%d]\n", irq);
    }

    tiantong_data->current_irq = irq;

    mod_delayed_work(system_wq, &tiantong_data->debounce_work, msecs_to_jiffies(tiantong_data->debounce_time));

    return IRQ_HANDLED;
}

static int gpio_tiantong_reuqest_gpio(struct platform_device *pdev,
    struct tiantong_gpio_data * tiantong_data,
    const char* gpio_name)
{
    int i;
    int idx = -1;
    int ret = 0;
    int gpio_num = -1;
    struct device *dev = &pdev->dev;
    struct device_node *np = dev->of_node;

    for(i = 0; i < TIANTONG_GPIO_MAX; i++) {
        if(!strcmp(gpio_name, tiantong_gpio_names[i])) {
            idx = i;
            break;
        }
    }

    if(idx == -1) {
        dev_err(dev, "Invalid gpio config name: %s",gpio_name);
        return -1;
    }

    gpio_num = of_get_named_gpio(np, gpio_name, 0);
    if (gpio_num < 0) {
        dev_err(dev, "Failed to get gpio %s, error: %d.\n", gpio_name, gpio_num);
    }
    ret = devm_gpio_request(dev, gpio_num, gpio_name);
    if (ret) {
        dev_err(dev, "Request gpio failed %s, error: %d.\n",  gpio_name, gpio_num);
        return ret;
    }
    dev_dbg(dev, "tiantong_gpio %s, #%u.\n", gpio_name, gpio_num);
    tiantong_data->data[idx].gpio_num = gpio_num;

    if(!strcmp(gpio_name, tiantong_gpio_names[TIANTONG_GPIO_BP_SLEEP_AP])) {
        gpio_direction_input(gpio_num);
        tiantong_data->data[idx].irq = gpio_to_irq(gpio_num);
        tiantong_data->data[idx].gpio_status = gpio_get_value(tiantong_data->data[idx].gpio_num);
        ret = devm_request_threaded_irq(dev, tiantong_data->data[idx].irq, NULL, gpio_tiantong_threaded_irq_handler,
            IRQF_ONESHOT |IRQF_TRIGGER_FALLING, "gpio_tiantong", tiantong_data);
        if (ret < 0) {
            dev_err(dev, "Failed to request irq.\n");
            return -EINVAL;
        }
        dev_pm_set_wake_irq(dev, tiantong_data->data[idx].irq);
    } else if(!strcmp(gpio_name, tiantong_gpio_names[TIANTONG_GPIO_PMIC_IRQ])) {
        gpio_direction_input(gpio_num);
        tiantong_data->data[idx].irq = gpio_to_irq(gpio_num);
        tiantong_data->data[idx].gpio_status = gpio_get_value(tiantong_data->data[idx].gpio_num);
        ret = devm_request_threaded_irq(dev, tiantong_data->data[idx].irq, NULL, gpio_tiantong_threaded_irq_handler,
            IRQF_ONESHOT |IRQF_TRIGGER_RISING, "gpio_tiantong_pmic", tiantong_data);
        if (ret < 0) {
            dev_err(dev, "Failed to request irq.\n");
            return -EINVAL;
        }
    }

    return ret;
}


static int gpio_tiantong_reuqest_regulator(struct platform_device *pdev,
    struct tiantong_gpio_data * tiantong_data)
{
    int ret_val = 0;
    struct device *dev = &pdev->dev;

    int index = 0;
    for(index = 0; index < TIANTONG_REG_PWR_MAX; index++) {

        if(index == TIANTONG_REG_PWR_CTL_TT_VDD_SIM_3P0) {
            dev_info(dev, "sim 3.0 and 1.8 same ldo");
            tiantong_data->tt_regulator_array[index] = tiantong_data->tt_regulator_array[TIANTONG_REG_PWR_CTL_TT_VDD_SIM_1P8];
            continue;
        }

        tiantong_data->tt_regulator_array[index] = regulator_get(dev, tiantong_regulator_name_array[index]);
        dev_info(dev, "%s: regulator_get index:%d regulator %p\n", __func__, index, tiantong_data->tt_regulator_array[index]);
        if (IS_ERR(tiantong_data->tt_regulator_array[index])) {
            dev_info(dev, "%s: Failed to get power regulator %s\n", __func__, tiantong_regulator_name_array[index]);
            ret_val = PTR_ERR(tiantong_data->tt_regulator_array[index]);
            break;
        }
    }

    dev_info(dev, "%s: index %d\n", __func__, index);
    if(index != TIANTONG_REG_PWR_MAX) {
        for(int idx = 0; idx < index; idx++) {

            if(idx == TIANTONG_REG_PWR_CTL_TT_VDD_SIM_3P0) {
                dev_info(dev, "sim 3.0 and 1.8 same ldo");
                tiantong_data->tt_regulator_array[idx] = tiantong_data->tt_regulator_array[TIANTONG_REG_PWR_CTL_TT_VDD_SIM_1P8];
                continue;
            }

            if (tiantong_data->tt_regulator_array[idx]) {
                regulator_put(tiantong_data->tt_regulator_array[idx]);
                tiantong_data->tt_regulator_array[idx] = NULL;
                dev_info(dev, "%s: free regulator %s\n", __func__, tiantong_regulator_name_array[idx]);
            }
        }
    } else {
        // disable all regulator in case the regulator enabled before device reboot.
        for(index = 0; index < TIANTONG_REG_PWR_MAX; index++) {
            if(index == TIANTONG_REG_PWR_CTL_TT_VDD_SIM_3P0) {
                dev_info(dev, "init sim 3.0 and 1.8 same ldo skip");
                continue;
            }
            if(regulator_is_enabled(tiantong_data->tt_regulator_array[index])) {
                ret_val = regulator_disable(tiantong_data->tt_regulator_array[index]);
                dev_info(dev, "init tiantong disable %s ret %d.\n", tiantong_regulator_name_array[index], ret_val);
            } else {
                dev_info(dev, "init tiantong %s already disabled\n", tiantong_regulator_name_array[index]);
            }
        }
    }

    return ret_val;
}


static int tiantong_msc06a_gpio_fops_open(struct inode *inode, struct file *file)
{
    int ret = 0;
    struct tiantong_gpio_data *tiantong_data = container_of(inode->i_cdev,
                        struct tiantong_gpio_data,
                        cdev);
    struct device *dev = tiantong_data->chardev;
    pr_info("Inside %s\n", __func__);
    get_device(dev);
    return ret;
}

static int tiantong_msc06a_gpio_fops_release(struct inode *inode, struct file *file)
{
    struct tiantong_gpio_data *tiantong_data = container_of(inode->i_cdev,
                        struct tiantong_gpio_data,
                        cdev);
    struct device *dev = tiantong_data->chardev;
    pr_info("Inside %s\n", __func__);
    put_device(dev);
    return 0;
}

static int tiantong_fasync(int fd, struct file *filp, int mode)
{
    struct tiantong_gpio_data *tiantong_data =
                        container_of(filp->f_inode->i_cdev, struct tiantong_gpio_data, cdev);
    //pr_info("tiantong_fasync\n");
    int ret;
    ret = fasync_helper(fd, filp, mode, &tiantong_data->async);
    pr_info("Inside %s\n", __func__);
    pr_debug("ret = %d\n", ret);
    return ret;
}

static int tiantong_power_on(struct tiantong_gpio_data *tiantong_data)
{
    int ret = -1;
    int success = 0;
    for(int index = 0; index < TIANTONG_REG_PWR_MAX; index++) {

        // ignore 3.0V for sim
        if(index == TIANTONG_REG_PWR_CTL_TT_VDD_SIM_3P0) {
            pr_info("skip 3p0\n");
            continue;
        }

        if (tiantong_data->tt_regulator_array[index]) {

            if(!regulator_is_enabled(tiantong_data->tt_regulator_array[index])) {
                regulator_set_voltage(tiantong_data->tt_regulator_array[index], reg_voltage[index].enable_uv, reg_voltage[index].enable_uv);
                ret = regulator_enable(tiantong_data->tt_regulator_array[index]);
                pr_info("tiantong enable %s ret %d.\n", tiantong_regulator_name_array[index], ret);
            } else {
                pr_info("tiantong %s already enabled\n", tiantong_regulator_name_array[index]);
            }
        } else {
            pr_info("tiantong enable %s invalied\n", tiantong_regulator_name_array[index]);
        }

        if(ret) {
            success = -1;
        }
    }

    return success;
}

static int tiantong_power_off(struct tiantong_gpio_data *tiantong_data)
{
    int ret = -1;
    int success = 0;
    for(int index = 0; index < TIANTONG_REG_PWR_MAX; index++) {
        if (tiantong_data->tt_regulator_array[index]) {
            if(regulator_is_enabled(tiantong_data->tt_regulator_array[index])) {
                ret = regulator_disable(tiantong_data->tt_regulator_array[index]);
                regulator_set_voltage(tiantong_data->tt_regulator_array[index], reg_voltage[index].disable_uv, reg_voltage[index].disable_uv);
                pr_info("tiantong disable %s ret %d.\n", tiantong_regulator_name_array[index], ret);
            } else {
                pr_info("tiantong %s already disabled\n", tiantong_regulator_name_array[index]);
            }
        } else {
            pr_info("tiantong disable %s invalied\n", tiantong_regulator_name_array[index]);
        }

        if(ret) {
            success = -1;
        }

    }

    return success;
}

static long tiantong_msc06a_gpio_fops_ioctl(struct file *file, unsigned int ioctl_num,
                unsigned long __user ioctl_param)
{
    int ret = 0;

//  void __user *argp = (void __user *)ioctl_param;
//  int __user *p = argp;
    //struct tt_chip_ldo_voltage_get_value __user *p_get_voltage = argp;
//  struct tt_chip_ldo_voltage_set_value  __user *p_set_voltage = argp;
//  int gpio_num;
//  int gpio_value = 0;
//  int tt_voltage_type = 0;
    //struct tt_chip_ldo_voltage_get_value voltage_get_buf;
//  struct tt_chip_ldo_voltage_set_value voltage_set_buf;

    struct tiantong_gpio_data *tiantong_data =
            container_of(file->f_inode->i_cdev, struct tiantong_gpio_data, cdev);
//  struct tt_chip_ldo_state ldo_data;

    pr_info("%s ioctl num %u\n", __func__, ioctl_num);
    switch (ioctl_num) {
        case TT_POWER_ON:
            ret = tiantong_power_on(tiantong_data);
            break;
        case TT_POWER_OFF:
            ret = tiantong_power_off(tiantong_data);
            break;
        default:
            pr_err("%s Entered default. Invalid ioctl num %u", __func__, ioctl_num);
            ret = -EINVAL;
    }
    return ret;
}

static const struct file_operations tiantong_msc06a_gpio_fops = {
    .owner = THIS_MODULE,
    .open = tiantong_msc06a_gpio_fops_open,
    .release = tiantong_msc06a_gpio_fops_release,
    .unlocked_ioctl = tiantong_msc06a_gpio_fops_ioctl,
    .fasync = tiantong_fasync,  // zhh tongzhi
};


static int gpio_tiantong_reg_chrdev(struct tiantong_gpio_data * tiantong_data)
{
    int ret = 0;

    tiantong_data->driver_name = "msc06a_chip";
    ret = alloc_chrdev_region(&tiantong_data->tiantong_major, 0,
                MINOR_NUMBER_COUNT, tiantong_data->driver_name);
    if (ret < 0) {
        pr_err("%s alloc_chr_dev_region failed ret : %d\n",
            __func__, ret);
        return ret;
    }
    pr_info("%s major number %d", __func__, MAJOR(tiantong_data->tiantong_major));

#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
    tiantong_data->tiantong_class = class_create(tiantong_data->driver_name);
#else
    tiantong_data->tiantong_class = class_create(THIS_MODULE, tiantong_data->driver_name);
#endif

    if (IS_ERR(tiantong_data->tiantong_class)) {
        ret = PTR_ERR(tiantong_data->tiantong_class);
        pr_err("%s class create failed. ret : %d", __func__, ret);
        goto err_class;
    }

    tiantong_data->chardev = device_create(tiantong_data->tiantong_class, NULL,
                tiantong_data->tiantong_major, NULL,
                tiantong_data->driver_name);
    if (IS_ERR(tiantong_data->chardev)) {
        ret = PTR_ERR(tiantong_data->chardev);
        pr_err("%s device create failed ret : %d\n", __func__, ret);
        goto err_device;
    }

    cdev_init(&tiantong_data->cdev, &tiantong_msc06a_gpio_fops);
    ret = cdev_add(&tiantong_data->cdev, tiantong_data->tiantong_major, 1);
    if (ret) {
        pr_err("%s cdev add failed, ret : %d\n", __func__, ret);
        goto err_cdev;
    }
    return ret;

err_cdev:
    device_destroy(tiantong_data->tiantong_class, tiantong_data->tiantong_major);
err_device:
    class_destroy(tiantong_data->tiantong_class);
err_class:
    unregister_chrdev_region(0, MINOR_NUMBER_COUNT);
    return ret;
}


static int gpio_tiantong_probe(struct platform_device *pdev)
{
    int i;
    int ret = 0;
    struct device *dev = &pdev->dev;
    struct device_node *np = dev->of_node;
    struct tiantong_gpio_data *tiantong_data;

    pr_info("%s enter\n", __func__);

    tiantong_data = devm_kzalloc(dev, sizeof(struct tiantong_gpio_data), GFP_KERNEL);
    if (!tiantong_data)
        return -ENOMEM;

    tiantong_data->data = devm_kcalloc(dev, TIANTONG_GPIO_MAX, sizeof(struct gpio_data), GFP_KERNEL);
    if (!tiantong_data->data)
        return -ENOMEM;

    mutex_init(&tiantong_data->lock);
    tiantong_data->dev = dev;
    platform_set_drvdata(pdev, tiantong_data);

    for(i = 0; i < TIANTONG_GPIO_MAX; i++) {
        gpio_tiantong_reuqest_gpio(pdev, tiantong_data, tiantong_gpio_names[i]);
    }

    gpio_tiantong_reuqest_regulator(pdev, tiantong_data);

    ret = sysfs_create_group(&dev->kobj, &attribute_group);
    if (ret < 0) {
        dev_err(dev, "Failed to create sysfs node.\n");
        return -EINVAL;
    }

    if ( of_property_read_u32(np, "debounce-time", &tiantong_data->debounce_time) ) {
        dev_info(dev, "Failed to get debounce-time, use default.\n");
        tiantong_data->debounce_time = 5;
    }

    device_init_wakeup(dev, true);
    INIT_DELAYED_WORK(&tiantong_data->debounce_work, gpio_debounce_work);

    ret = gpio_tiantong_reg_chrdev(tiantong_data);
    if (ret) {
        pr_err("%s register char dev failed, rc : %d", __func__, ret);
        return ret;
    }

    return ret;
}

static int gpio_tiantong_remove(struct platform_device *pdev)
{
    struct tiantong_gpio_data *tiantong_data = platform_get_drvdata(pdev);

    //sysfs_remove_group(&pdev->dev.kobj, &attribute_group);
    //cancel_delayed_work_sync(&tiantong_data->debounce_work);
    dev_pm_clear_wake_irq(tiantong_data->dev);

    mutex_destroy(&tiantong_data->lock);

    return 0;
}

static const struct of_device_id gpio_tiantong_msc06a_of_match[] = {
    { .compatible = "mdm,gpio-tiantong-msc06a", },
    {},
};

#ifdef CONFIG_PM
static int gpio_tiantong_suspend(struct device *dev)
{
    int gpio, rc;
    struct tiantong_gpio_data *tiantong_data;

    tiantong_data = dev_get_drvdata(dev);
    gpio = tiantong_data->data[TIANTONG_GPIO_AP_SLEEP_BP].gpio_num;

    rc = gpio_direction_output(gpio, 1);
    dev_info(dev, "tiantong_suspend, %d.\n", rc);

    return 0;
}

static int gpio_tiantong_resume(struct device *dev)
{
    int gpio, rc;
    struct tiantong_gpio_data *tiantong_data;

    tiantong_data = dev_get_drvdata(dev);
    gpio = tiantong_data->data[TIANTONG_GPIO_AP_SLEEP_BP].gpio_num;

    rc = gpio_direction_output(gpio, 0);
    dev_info(dev, "tiantong_resume, %d.\n", rc);

    return 0;
}

static const struct dev_pm_ops gpio_tiantong_pm_ops = {
    .suspend = gpio_tiantong_suspend,
    .resume = gpio_tiantong_resume,
    .freeze = gpio_tiantong_suspend,
    .restore = gpio_tiantong_resume,
};
#endif

static struct platform_driver gpio_tiantong_msc06a_driver = {
    .driver = {
        .name = "gpio-tiantong-msc06a",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(gpio_tiantong_msc06a_of_match),
#ifdef CONFIG_PM
        .pm = &gpio_tiantong_pm_ops,
#endif
    },
    .probe = gpio_tiantong_probe,
    .remove = gpio_tiantong_remove,
};

module_platform_driver(gpio_tiantong_msc06a_driver);
MODULE_DESCRIPTION("Driver for tiantong GPIO control");
MODULE_LICENSE("GPL");


