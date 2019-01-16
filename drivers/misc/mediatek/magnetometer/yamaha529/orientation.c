/*
 * Copyright (c) 2010 Yamaha Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/slab.h>



#define MEDIATEK_CODE

#ifdef MEDIATEK_CODE
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#define OSENSOR 0x85
#define OSENSOR_IOCTL_READ_POSTUREDATA _IO(OSENSOR, 0x01)
#endif

/* for debugging */
#define DEBUG 0

#define SENSOR_TYPE (3)

#if SENSOR_TYPE == 1
#define SENSOR_NAME "accelerometer"
#elif SENSOR_TYPE == 2
#define SENSOR_NAME "geomagnetic"
#elif SENSOR_TYPE == 3
#define SENSOR_NAME "orientation"
#elif SENSOR_TYPE == 4
#define SENSOR_NAME "gyroscope"
#elif SENSOR_TYPE == 5
#define SENSOR_NAME "light"
#elif SENSOR_TYPE == 6
#define SENSOR_NAME "pressure"
#elif SENSOR_TYPE == 7
#define SENSOR_NAME "temperature"
#elif SENSOR_TYPE == 8
#define SENSOR_NAME "proximity"
#endif

#define SENSOR_DEFAULT_DELAY            (200)   /* 200 ms */
#define SENSOR_MAX_DELAY                (2000)  /* 2000 ms */
#define ABS_STATUS                      (ABS_BRAKE)
#define ABS_WAKE                        (ABS_MISC)
#define ABS_CONTROL_REPORT              (ABS_THROTTLE)

static int suspend(void);
static int resume(void);

struct sensor_data {
    struct mutex mutex;
    int enabled;
    int delay;
#if DEBUG
    int suspend;
#endif
};

static struct platform_device *sensor_pdev = NULL;
static struct input_dev *this_data = NULL;

static int
suspend(void)
{
    /* implement suspend of the sensor */
    printk(KERN_DEBUG "%s: suspend\n", SENSOR_NAME);

    if (strcmp(SENSOR_NAME, "gyroscope") == 0) {
        /* suspend gyroscope */
    }
    else if (strcmp(SENSOR_NAME, "light") == 0) {
        /* suspend light */
    }
    else if (strcmp(SENSOR_NAME, "pressure") == 0) {
        /* suspend pressure */
    }
    else if (strcmp(SENSOR_NAME, "temperature") == 0) {
        /* suspend temperature */
    }
    else if (strcmp(SENSOR_NAME, "proximity") == 0) {
        /* suspend proximity */
    }

    return 0;
}

static int
resume(void)
{
    /* implement resume of the sensor */
    printk(KERN_DEBUG "%s: resume\n", SENSOR_NAME);

    if (strcmp(SENSOR_NAME, "gyroscope") == 0) {
        /* resume gyroscope */
    }
    else if (strcmp(SENSOR_NAME, "light") == 0) {
        /* resume light */
    }
    else if (strcmp(SENSOR_NAME, "pressure") == 0) {
        /* resume pressure */
    }
    else if (strcmp(SENSOR_NAME, "temperature") == 0) {
        /* resume temperature */
    }
    else if (strcmp(SENSOR_NAME, "proximity") == 0) {
        /* resume proximity */
    }

#if DEBUG
    {
        struct sensor_data *data = input_get_drvdata(this_data);
        data->suspend = 0;
    }
#endif /* DEBUG */

    return 0;
}

static
int
yas529_osensor_get_delay(void)
{
    struct sensor_data *data = input_get_drvdata(this_data);
    int delay;

    mutex_lock(&data->mutex);

    delay = data->delay;

    mutex_unlock(&data->mutex);

    return delay;
}


/* Sysfs interface */
static ssize_t
sensor_delay_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    return sprintf(buf, "%d\n", yas529_osensor_get_delay());
}


static
int
yas529_osensor_set_delay(int delay)
{
    struct sensor_data *data = input_get_drvdata(this_data);

    if (delay < 0) {
        return -1;
    }

    if (SENSOR_MAX_DELAY < delay) {
        delay = SENSOR_MAX_DELAY;
    }

    mutex_lock(&data->mutex);

    data->delay = delay;

    input_report_abs(this_data, ABS_CONTROL_REPORT, (data->enabled<<16) | delay);

    mutex_unlock(&data->mutex);

    return 0;
}


static ssize_t
sensor_delay_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    int value = simple_strtoul(buf, NULL, 10);

    if (value < 0) {
        return count;
    }
    yas529_osensor_set_delay(value);

    return count;
}


static
int
yas529_osensor_get_enable(void)
{
    struct sensor_data *data = input_get_drvdata(this_data);
    int enabled;

    mutex_lock(&data->mutex);

    enabled = data->enabled;

    mutex_unlock(&data->mutex);

    return enabled;
}


static ssize_t
sensor_enable_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    return sprintf(buf, "%d\n", yas529_osensor_get_enable());
}


static
int
yas529_osensor_set_enable(int enable)
{
    struct sensor_data *data = input_get_drvdata(this_data);

    mutex_lock(&data->mutex);

    if (data->enabled && !enable) {
        suspend();
    }
    if (!data->enabled && enable) {
        resume();
    }
    data->enabled = enable;

    input_report_abs(this_data, ABS_CONTROL_REPORT, (enable<<16) | data->delay);

    mutex_unlock(&data->mutex);

    return 0;
}


static ssize_t
sensor_enable_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    int value = simple_strtoul(buf, NULL, 10);

    if (value != 0 && value != 1) {
        return count;
    }

    yas529_osensor_set_enable(value);

    return count;
}

static ssize_t
sensor_wake_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    static int cnt = 1;

    input_report_abs(input_data, ABS_WAKE, cnt++);

    return count;
}

static ssize_t
sensor_data_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    unsigned long flags;
#if SENSOR_TYPE <= 4
    int x, y, z;
#else
    int x;
#endif

    spin_lock_irqsave(&input_data->event_lock, flags);

    x = input_data->abs[ABS_X];
#if SENSOR_TYPE <= 4
    y = input_data->abs[ABS_Y];
    z = input_data->abs[ABS_Z];
#endif

    spin_unlock_irqrestore(&input_data->event_lock, flags);

#if SENSOR_TYPE <= 4
    return sprintf(buf, "%d %d %d\n", x, y, z);
#else
    return sprintf(buf, "%d\n", x);
#endif
}

static ssize_t
sensor_status_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    unsigned long flags;
    int status;

    spin_lock_irqsave(&input_data->event_lock, flags);

    status = input_data->abs[ABS_STATUS];

    spin_unlock_irqrestore(&input_data->event_lock, flags);

    return sprintf(buf, "%d\n", status);
}

#if DEBUG

static ssize_t sensor_debug_suspend_show(struct device *dev,
                                         struct device_attribute *attr, char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct sensor_data *data = input_get_drvdata(input_data);

    return sprintf(buf, "%d\n", data->suspend);
}

static ssize_t sensor_debug_suspend_store(struct device *dev,
                                          struct device_attribute *attr,
                                          const char *buf, size_t count)
{
    unsigned long value = simple_strtoul(buf, NULL, 10);

    if (value) {
        suspend();
    } else {
        resume();
    }

    return count;
}
#endif /* DEBUG */

static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,
        sensor_delay_show, sensor_delay_store);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
        sensor_enable_show, sensor_enable_store);
static DEVICE_ATTR(wake, S_IWUSR|S_IWGRP,
        NULL, sensor_wake_store);
static DEVICE_ATTR(data, S_IRUGO, sensor_data_show, NULL);
static DEVICE_ATTR(status, S_IRUGO, sensor_status_show, NULL);

#if DEBUG
static DEVICE_ATTR(debug_suspend, S_IRUGO|S_IWUSR,
                   sensor_debug_suspend_show, sensor_debug_suspend_store);
#endif /* DEBUG */

static struct attribute *sensor_attributes[] = {
    &dev_attr_delay.attr,
    &dev_attr_enable.attr,
    &dev_attr_wake.attr,
    &dev_attr_data.attr,
    &dev_attr_status.attr,
#if DEBUG
    &dev_attr_debug_suspend.attr,
#endif /* DEBUG */
    NULL
};

static struct attribute_group sensor_attribute_group = {
    .attrs = sensor_attributes
};

static int
sensor_suspend(struct platform_device *pdev, pm_message_t state)
{
    struct sensor_data *data = input_get_drvdata(this_data);
    int rt = 0;

    mutex_lock(&data->mutex);

    if (data->enabled) {
        rt = suspend();
    }

    mutex_unlock(&data->mutex);

    return rt;
}

static int
sensor_resume(struct platform_device *pdev)
{
    struct sensor_data *data = input_get_drvdata(this_data);
    int rt = 0;

    mutex_lock(&data->mutex);

    if (data->enabled) {
        rt = resume();
    }

    mutex_unlock(&data->mutex);

    return rt;
}

#ifdef MEDIATEK_CODE

static int
ioctl_read_posturedata(unsigned long args)
{
    unsigned long flags;
    struct input_dev *input_data = this_data;
    int buf[4], *p;

    p = (int*) args;

    spin_lock_irqsave(&input_data->event_lock, flags);

    buf[0] = input_data->abs[ABS_X] / 1000;
    buf[1] = input_data->abs[ABS_Y] / 1000;
    buf[2] = input_data->abs[ABS_Z] / 1000;
    buf[3] = input_data->abs[ABS_STATUS];

    spin_unlock_irqrestore(&input_data->event_lock, flags);

    if (copy_to_user(p, buf, sizeof(buf))) {
        return -EFAULT;
    }

    return 0;
}

/*
static int
orientation_dev_ioctl(struct inode *node, struct file *fp, unsigned int cmd,
        unsigned long args)
{
    int result = 0;

    switch (cmd) {
    case OSENSOR_IOCTL_READ_POSTUREDATA:
        result = ioctl_read_posturedata(args);
        break;
    default:
        result = -ENOTTY;
        break;
    }

    return result;
}

static struct file_operations orientation_fileops = {
    .owner      = THIS_MODULE,
    .ioctl      = orientation_dev_ioctl,
};

static struct miscdevice orientation_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "osensor",
    .fops  = &orientation_fileops,
};
*/

#endif

/*----------------------------------------------------------------------------*/
int yamaha529_orientation_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay, status;
	hwm_sensor_data* osensor_data;
	unsigned long flags;
	struct input_dev *input_data = this_data;


	printk("yamaha529_orientation_operate!\n");
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				printk("Set delay parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value <= 20)
				{
					sample_delay = 20;
				}
				
				
				yas529_osensor_set_delay(sample_delay);
			}	
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				printk("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				yas529_osensor_set_enable(value);
				// Do nothing
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				printk("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				osensor_data = (hwm_sensor_data *)buff_out;
				spin_lock_irqsave(&input_data->event_lock, flags);

				osensor_data->values[0] = input_data->abs[ABS_X];
				osensor_data->values[1] = input_data->abs[ABS_Y];
				osensor_data->values[2] = input_data->abs[ABS_Z];
				osensor_data->status = input_data->abs[ABS_STATUS];

				spin_unlock_irqrestore(&input_data->event_lock, flags);			 
				
				
				osensor_data->value_divide = 1000;				
			}

			break;
		default:
			printk("gsensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;

}


static int
sensor_probe(struct platform_device *pdev)
{
    struct sensor_data *data = NULL;
    struct input_dev *input_data = NULL;
    int input_registered = 0, sysfs_created = 0;
    int rt;
printk("orientation sensor_probe!\n");
	struct hwmsen_object sobj;
#ifdef MEDIATEK_CODE
    int misc_registered = 0;
#endif

    data = kzalloc(sizeof(struct sensor_data), GFP_KERNEL);
    if (!data) {
        rt = -ENOMEM;
        goto err;
    }
    data->enabled = 0;
    data->delay = SENSOR_DEFAULT_DELAY;

    input_data = input_allocate_device();
    if (!input_data) {
        rt = -ENOMEM;
        printk(KERN_ERR
               "sensor_probe: Failed to allocate input_data device\n");
        goto err;
    }

    set_bit(EV_ABS, input_data->evbit);
    input_set_capability(input_data, EV_ABS, ABS_X);
#if SENSOR_TYPE <= 4
    input_set_capability(input_data, EV_ABS, ABS_Y);
    input_set_capability(input_data, EV_ABS, ABS_Z);
#endif
    input_set_capability(input_data, EV_ABS, ABS_STATUS); /* status */
    input_set_capability(input_data, EV_ABS, ABS_WAKE); /* wake */
    input_set_capability(input_data, EV_ABS, ABS_CONTROL_REPORT); /* enabled/delay */
    input_data->name = SENSOR_NAME;

    rt = input_register_device(input_data);
    if (rt) {
        printk(KERN_ERR
               "sensor_probe: Unable to register input_data device: %s\n",
               input_data->name);
        goto err;
    }
    input_set_drvdata(input_data, data);
    input_registered = 1;

    rt = sysfs_create_group(&input_data->dev.kobj,
            &sensor_attribute_group);
    if (rt) {
        printk(KERN_ERR
               "sensor_probe: sysfs_create_group failed[%s]\n",
               input_data->name);
        goto err;
    }
    sysfs_created = 1;

#ifdef MEDIATEK_CODE
/*
    if ((rt = misc_register(&orientation_device)) < 0) {
        printk(KERN_ERR "misc_register failed[%d]\n", rt);
        goto err;
    }
    misc_registered = 1;
    */
#endif

	sobj.self = data;
    sobj.polling = 1;
    sobj.sensor_operate = yamaha529_orientation_operate;
	if(rt = hwmsen_attach(ID_ORIENTATION, &sobj))
	{
		printk("attach fail = %d\n", rt);
		goto err;
	}

    mutex_init(&data->mutex);
    this_data = input_data;

    return 0;

err:
    if (data != NULL) {
#ifdef MEDIATEK_CODE
/*
        if (misc_registered) {
            misc_deregister(&orientation_device);
        }
        */
#endif
        if (input_data != NULL) {
            if (sysfs_created) {
                sysfs_remove_group(&input_data->dev.kobj,
                        &sensor_attribute_group);
            }
            if (input_registered) {
                input_unregister_device(input_data);
            }
            else {
                input_free_device(input_data);
            }
            input_data = NULL;
        }
        kfree(data);
    }

    return rt;
}

static int
sensor_remove(struct platform_device *pdev)
{
    struct sensor_data *data;

    if (this_data != NULL) {
        data = input_get_drvdata(this_data);
        sysfs_remove_group(&this_data->dev.kobj,
                &sensor_attribute_group);
        input_unregister_device(this_data);
        if (data != NULL) {
            kfree(data);
        }
    }

    return 0;
}

/*
 * Module init and exit
 */
static struct platform_driver sensor_driver = {
    .probe      = sensor_probe,
    .remove     = sensor_remove,
    .suspend    = sensor_suspend,
    .resume     = sensor_resume,
    .driver = {
        .name   = SENSOR_NAME,
        .owner  = THIS_MODULE,
    },
};

static int __init sensor_init(void)
{
    
	if(platform_driver_register(&sensor_driver))
	{
		printk("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
module_init(sensor_init);

static void __exit sensor_exit(void)
{
	platform_driver_unregister(&sensor_driver);
}
module_exit(sensor_exit);

MODULE_AUTHOR("Yamaha Corporation");
MODULE_LICENSE( "GPL" );
MODULE_VERSION("1.2.0");
