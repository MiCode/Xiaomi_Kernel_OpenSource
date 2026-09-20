#include <linux/thermal.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * P19 Thermal Monitor Module
 *
 * Combines MSM, QUIET, and PA2 temperature monitoring into a single module
 */
#define P19_CLASS_NAME  "thermal_factory"
struct class *p19_class = NULL;
EXPORT_SYMBOL_GPL(p19_class);
enum p19_device_type {
    DEVICE_MSM,
    DEVICE_QUIET,
    DEVICE_PA2,
    DEVICE_MAX
};
struct p19_device_info {
    struct device *device;
    const char *device_name;
    const char *thermal_zone_name;
};
static struct p19_device_info p19_devices[DEVICE_MAX];
static ssize_t show_temp(struct device *dev, struct device_attribute *attr, char *buf)
{
    int temp = 0;
    struct thermal_zone_device *thermal_dev;
    int ret = 0;
    int i;
    
    for (i = 0; i < DEVICE_MAX; i++) {
        if (p19_devices[i].device == dev)
            break;
    }
    
    if (i == DEVICE_MAX) {
        printk(" %s %s %d: Unknown device!\n", __FUNCTION__, __FILE__, __LINE__);
        return -EINVAL;
    }
    
    thermal_dev = thermal_zone_get_zone_by_name(p19_devices[i].thermal_zone_name);
    if (thermal_dev == NULL) {
        printk(" %s %s %d thermal_dev is null for %s!\n", 
               __FUNCTION__, __FILE__, __LINE__, p19_devices[i].device_name);
        return 0;
    }
    
    ret = thermal_zone_get_temp(thermal_dev, &temp);
    if (ret) {
        printk(" %s %s %d thermal_zone_get_temp error for %s!\n", 
               __FUNCTION__, __FILE__, __LINE__, p19_devices[i].device_name);
    }
    
    printk("%s: %s_temp=%d \n", __func__, p19_devices[i].device_name, temp);
    return sprintf(buf, "%d\n", temp);
}
static ssize_t store_temp(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int temp;
    if (kstrtoint(buf, 10, &temp) != 0)
        return -EINVAL;

    return count;
}
static DEVICE_ATTR(msm_temp, 0664, show_temp, store_temp);
static DEVICE_ATTR(quiet_temp, 0664, show_temp, store_temp);
static DEVICE_ATTR(pa2_temp, 0664, show_temp, store_temp);
static struct device_attribute *p19_dev_attrs[DEVICE_MAX] = {
    &dev_attr_msm_temp,
    &dev_attr_quiet_temp,
    &dev_attr_pa2_temp
};
static int init_device(enum p19_device_type type)
{
    int result = 0;
    p19_devices[type].device = kzalloc(sizeof(struct device), GFP_KERNEL);
    if (!p19_devices[type].device) {
        printk(KERN_INFO "ERR: kzalloc /sys/class/%s/%s fail! %s %d %s.\n",
               P19_CLASS_NAME, p19_devices[type].device_name, 
               __FILE__, __LINE__, __FUNCTION__);
        return -ENOMEM;
    }
    p19_devices[type].device->init_name = p19_devices[type].device_name;
    p19_devices[type].device->class = p19_class;
    p19_devices[type].device->release = (void(*)(struct device*))kfree;
    result = device_register(p19_devices[type].device);
    if (result) {
        printk(KERN_INFO "ERR: /sys/class/%s/%s register fail! %s %d %s.\n",
               P19_CLASS_NAME, p19_devices[type].device_name, 
               __FILE__, __LINE__, __FUNCTION__);
        kfree(p19_devices[type].device);
        return result;
    }
    result = device_create_file(p19_devices[type].device, p19_dev_attrs[type]);
    if (result) {
        printk(KERN_INFO "ERR: /sys/class/%s/%s create file fail! %s %d %s.\n",
               P19_CLASS_NAME, p19_devices[type].device_name, 
               __FILE__, __LINE__, __FUNCTION__);
        device_unregister(p19_devices[type].device);
        kfree(p19_devices[type].device);
        return result;
    }
    
    return 0;
}
static void cleanup_device(enum p19_device_type type)
{
    if (p19_devices[type].device) {
        device_remove_file(p19_devices[type].device, p19_dev_attrs[type]);
        device_unregister(p19_devices[type].device);
        kfree(p19_devices[type].device);
        p19_devices[type].device = NULL;
    }
}
static int __init p19_thermal_init(void)
{
    int result = 0;
    int i;
    

    printk(KERN_INFO "P19 Thermal Module Initializing...\n");
    
    p19_devices[DEVICE_MSM].device_name = "msm_temp";
    p19_devices[DEVICE_MSM].thermal_zone_name = "cpu_therm";
    
    p19_devices[DEVICE_QUIET].device_name = "quiet_temp";
    p19_devices[DEVICE_QUIET].thermal_zone_name = "quiet_therm";
    
    p19_devices[DEVICE_PA2].device_name = "pa2_temp";
    p19_devices[DEVICE_PA2].thermal_zone_name = "modem1_pa0";
    p19_class = class_create(P19_CLASS_NAME);
    if (IS_ERR(p19_class)) {
        result = PTR_ERR(p19_class);
        pr_err("Failed to create p19 class: %d\n", result);
        return result;
    }
    for (i = 0; i < DEVICE_MAX; i++) {
        result = init_device(i);
        if (result) {
            while (--i >= 0) {
                cleanup_device(i);
            }
            class_destroy(p19_class);
            p19_class = NULL;
            return result;
        }
    }
    
    printk(KERN_INFO "P19 Thermal Module Initialized Successfully\n");
    return 0;
}
static void __exit p19_thermal_exit(void)
{
    int i;
    printk(KERN_INFO "P19 Thermal Module Exiting...\n");
    
    for (i = 0; i < DEVICE_MAX; i++) {
        cleanup_device(i);
    }
    if (p19_class) {
        class_destroy(p19_class);
        p19_class = NULL;
    }
    
    printk(KERN_INFO "P19 Thermal Module Exited\n");
}
module_init(p19_thermal_init);
module_exit(p19_thermal_exit);
MODULE_LICENSE("GPL v2");
