/* drivers/input/touchscreen/gt1x.c
 *
 * 2010 - 2014 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version: 1.4
 * Release Date:  2015/07/10
 */

#include <linux/irq.h>
#include "gt1x.h"
#include "gt1x_generic.h"
#if GTP_ICS_SLOT_REPORT
#include <linux/input/mt.h>
#endif

/*shenwenbin add for show TP info for *#87# 20180120 begin*/
#include "../lct_tp_fm_info.h"
/*shenwenbin add for show TP info for *#87# 20180120 end*/



extern char g_lcd_id[128];

static struct work_struct gt1x_work;


static struct work_struct gt_resmue;
static struct work_struct gt_suspend;


struct input_dev *input_dev;
static struct workqueue_struct *gt1x_wq;
static const char *gt1x_ts_name = "goodix-ts";
static const char *input_dev_phys = "input/ts";

struct goodix_pinctrl gt_pinctrl;

#ifdef GTP_CONFIG_OF
int gt1x_rst_gpio;
int gt1x_int_gpio;
#endif


extern s32 gtp_test_sysfs_init(void);



/*shenwenbin add for runin 20180120 begin*/
extern int i2c_devinfo_device_write(char *buf);
/*shenwenbin add for runin 20180120 end*/

char gt1x_tp_lockdown_info[128];
unsigned char get_lockdown_info(struct i2c_client *client,char *pProjectCode);

static int gt1x_register_powermanger(void);
static int gt1x_unregister_powermanger(void);

/**
 * gt1x_i2c_write - i2c write.
 * @addr: register address.
 * @buffer: data buffer.
 * @len: the bytes of data to write.
 *Return: 0: success, otherwise: failed
 */
s32 gt1x_i2c_write(u16 addr, u8 *buffer, s32 len)
{
	struct i2c_msg msg = {
		.flags = 0,
		.addr = gt1x_i2c_client->addr,
	};
	return _do_i2c_write(&msg, addr, buffer, len);
}

/**
 * gt1x_i2c_read - i2c read.
 * @addr: register address.
 * @buffer: data buffer.
 * @len: the bytes of data to write.
 *Return: 0: success, otherwise: failed
 */
s32 gt1x_i2c_read(u16 addr, u8 *buffer, s32 len)
{
	u8 addr_buf[GTP_ADDR_LENGTH] = { (addr >> 8) & 0xFF, addr & 0xFF };
	struct i2c_msg msgs[2] = {
		{
		 .addr = gt1x_i2c_client->addr,
		 .flags = 0,
		 .buf = addr_buf,
		 .len = GTP_ADDR_LENGTH},
		{
		 .addr = gt1x_i2c_client->addr,
		 .flags = I2C_M_RD}
	};
	return _do_i2c_read(msgs, addr, buffer, len);
}

static spinlock_t irq_lock;
static s32 irq_is_disable;

/**
 * gt1x_irq_enable - enable irq function.
 *
 */
void gt1x_irq_enable(void)
{
	unsigned long irqflags = 0;

	GTP_DEBUG_FUNC();

	spin_lock_irqsave(&irq_lock, irqflags);
	if (irq_is_disable) {
		enable_irq(gt1x_i2c_client->irq);
		irq_is_disable = 0;
	}
	spin_unlock_irqrestore(&irq_lock, irqflags);
}

/**
 * gt1x_irq_enable - disable irq function.
 *
 */
void gt1x_irq_disable(void)
{
	unsigned long irqflags;

	GTP_DEBUG_FUNC();

	spin_lock_irqsave(&irq_lock, irqflags);
	if (!irq_is_disable) {
		irq_is_disable = 1;
		disable_irq_nosync(gt1x_i2c_client->irq);
	}
	spin_unlock_irqrestore(&irq_lock, irqflags);
}
/*add for gt1x_select_gesture_mode  by qujiong*/
int gt1x_select_gesture_mode(struct input_dev *dev,unsigned int type,unsigned int code,int value)
{
	GTP_DEBUG("[GESTURE]enter gt1x the gesture select!!!!");
	if ((type == EV_SYN)&& (code == SYN_CONFIG)){


		if (value == 5){



			gt1x_gesture_debug(1);
		}else {




			gt1x_gesture_debug(0);
		}


	}

	return 0;
}


static struct kobject *gt_tp_device;
static ssize_t gesture_store(struct device *dev, struct device_attribute *attr, const char *buff, size_t count)
{

    if ((strncmp(buff, "1", 1)  == 0))
    {

      gt1x_gesture_debug(1);
    }
	else
	{

      gt1x_gesture_debug(0);
	}
    return count;
}
static ssize_t gesture_show(struct device *dev, struct device_attribute *attr, char *buf)
{
     ssize_t count;
     count = sprintf(buf, "gt_gesture_enable =  %d\n", gesture_enabled);
     return count;
}
static DEVICE_ATTR(wakeup_gesture_enable, S_IRUGO|S_IWUSR, gesture_show, gesture_store);

static int tp_gesture_creat_sys_entry(void)
{
    int32_t rc = 0;

	gt_tp_device = kobject_create_and_add("android_touch", NULL);
	if (gt_tp_device == NULL) {

		rc = -ENOMEM;
		return rc ;
	}
	rc = sysfs_create_file(gt_tp_device, &dev_attr_wakeup_gesture_enable.attr);
	if (rc) {

		kobject_del(gt_tp_device);
	}
	printk(" %s: sysfs_create_file succeed\n", __func__);
	return 0 ;
}


unsigned char get_lockdown_info(struct i2c_client *client,char *pProjectCode)
{

        u8 reg_val[8] = {0};
        int ret;
        ret=gt1x_i2c_read(GTP_REG_LOCKDOWN,reg_val,8);
        if (ret<0){
                GTP_ERROR("i2c read lockdown info failed!!!\n");
        }

        sprintf(pProjectCode,"%02x%02x%02x%02x%02x%02x%02x%02x",reg_val[0],reg_val[1],reg_val[2],reg_val[3],reg_val[4],reg_val[5],reg_val[6],reg_val[7]);
        return 0;
}

static ssize_t goodix_lockdown_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        return snprintf(buf, 127, "%s\n", gt1x_tp_lockdown_info);
}

static ssize_t goodix_lockdown_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t size)
{
        if (size > 49)
                return -EINVAL;
        strlcpy(gt1x_tp_lockdown_info, buf, size);
        if (gt1x_tp_lockdown_info[size-1] == '\n')
                gt1x_tp_lockdown_info[size-1] = 0;
        return size;
}

static ssize_t goodix_fwversion_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{

        int count =0;







        count += sprintf(buf, "FW:%06X\n",gt1x_version.patch_id);
        return count;
}

static ssize_t goodix_fwversion_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t size)
{
        return -EPERM;
}

static DEVICE_ATTR(tp_lock_down_info, S_IWUSR|S_IRUGO,goodix_lockdown_show, goodix_lockdown_store);
static DEVICE_ATTR(fw_version, S_IWUSR|S_IRUGO,goodix_fwversion_show, goodix_fwversion_store);

/* add your attr in here*/
static struct attribute *goodix_attributes[] =
{
    &dev_attr_tp_lock_down_info.attr,
    &dev_attr_fw_version.attr,
    NULL
};

static struct attribute_group goodix_attribute_group =
{
    .attrs = goodix_attributes
};

/************************************************************************
* Name: goodix_create_sysfs
* Brief:  create sysfs for debug
* Input: i2c info
* Output: no
* Return: success =0
***********************************************************************/
int goodix_create_sysfs(struct i2c_client *client)
{
    int err;
    err = sysfs_create_group(&client->dev.kobj, &goodix_attribute_group);
    if (0 != err)
    {
        printk("[EX]: sysfs_create_group() failed!!");
        sysfs_remove_group(&client->dev.kobj, &goodix_attribute_group);
        return -EIO;
    }
    else
    {
        printk("[EX]: sysfs_create_group() succeeded!!");
    }
    return err;
}

/************************************************************************
* Name: goodix_remove_sysfs
* Brief:  remove sys
* Input: i2c info
* Output: no
* Return: no
***********************************************************************/

int goodix_remove_sysfs(struct i2c_client *client)
{
    sysfs_remove_group(&client->dev.kobj, &goodix_attribute_group);
    return 0;
}


#ifndef GTP_CONFIG_OF
int gt1x_power_switch(s32 state)
{
    return 0;
}
#endif

int gt1x_debug_proc(u8 *buf, int count)
{
	return -EPERM;
}

#if GTP_CHARGER_SWITCH
u32 gt1x_get_charger_status(void)
{
	return 0;

}
#endif

/**
 * gt1x_ts_irq_handler - External interrupt service routine for interrupt mode.
 * @irq:  interrupt number.
 * @dev_id: private data pointer.
 * Return: Handle Result.
 *  		IRQ_HANDLED: interrupt handled successfully
 */
static irqreturn_t gt1x_ts_irq_handler(int irq, void *dev_id)
{
	GTP_DEBUG_FUNC();
    gt1x_irq_disable();
	queue_work(gt1x_wq, &gt1x_work);
	return IRQ_HANDLED;
}

/**
 * gt1x_touch_down - Report touch point event .
 * @id: trackId
 * @x:  input x coordinate
 * @y:  input y coordinate
 * @w:  input pressure
 * Return: none.
 */
void gt1x_touch_down(s32 x, s32 y, s32 size, s32 id)
{
#if GTP_CHANGE_X2Y
	GTP_SWAP(x, y);
#endif

	input_report_key(input_dev, BTN_TOUCH, 1);
#if GTP_ICS_SLOT_REPORT
	input_mt_slot(input_dev, id);
	input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
#else
	input_report_abs(input_dev, ABS_MT_TRACKING_ID, id);
#endif

	input_report_abs(input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(input_dev, ABS_MT_PRESSURE, size+size/3);
	input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, size/10);

#if !GTP_ICS_SLOT_REPORT
	input_mt_sync(input_dev);
#endif
}

/**
 * gt1x_touch_up -  Report touch release event.
 * @id: trackId
 * Return: none.
 */
void gt1x_touch_up(s32 id)
{
#if GTP_ICS_SLOT_REPORT
	input_mt_slot(input_dev, id);
	input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
#else
	input_mt_sync(input_dev);
#endif
}

/**
 * gt1x_ts_work_func - Goodix touchscreen work function.
 * @iwork: work struct of gt1x_workqueue.
 * Return: none.
 */
static void gt1x_ts_work_func(struct work_struct *work)
{
	u8 end_cmd = 0;
	u8 finger = 0;
	s32 ret = 0;
	u8 point_data[11] = { 0 };

    if (update_info.status) {
        GTP_DEBUG("Ignore interrupts during fw update.");
        return;
    }

#if GTP_GESTURE_WAKEUP
	ret = gesture_event_handler(input_dev);
	if (ret >= 0) {
		goto exit_work_func;
	}
#endif

	if (gt1x_halt) {
		GTP_DEBUG("Ignore interrupts after suspend...");
        return;
	}

	ret = gt1x_i2c_read(GTP_READ_COOR_ADDR, point_data, sizeof(point_data));
	if (ret < 0) {
		GTP_ERROR("I2C transfer error!");
#if !GTP_ESD_PROTECT
		gt1x_power_reset();
#endif
		goto exit_work_func;
	}

	finger = point_data[0];
	if (finger == 0x00) {
		gt1x_request_event_handler();
	}

	if ((finger & 0x80) == 0) {
#if HOTKNOT_BLOCK_RW
		if (!hotknot_paired_flag)
#endif
		{

			goto exit_eint;
		}
	}
#if HOTKNOT_BLOCK_RW
	ret = hotknot_event_handler(point_data);
	if (!ret) {
		goto exit_work_func;
	}
#endif

#if GTP_PROXIMITY
	ret = gt1x_prox_event_handler(point_data);
	if (ret > 0) {
		goto exit_work_func;
	}
#endif

#if GTP_WITH_STYLUS
	ret = gt1x_touch_event_handler(point_data, input_dev, pen_dev);
#else
	ret = gt1x_touch_event_handler(point_data, input_dev, NULL);
#endif

exit_work_func:
	if (!gt1x_rawdiff_mode && (ret >= 0 || ret == ERROR_VALUE)) {
		ret = gt1x_i2c_write(GTP_READ_COOR_ADDR, &end_cmd, 1);
		if (ret < 0) {
			GTP_ERROR("I2C write end_cmd  error!");
		}
	}
exit_eint:
    gt1x_irq_enable();

}

/*
 * Devices Tree support,
*/
#ifdef GTP_CONFIG_OF

static struct regulator *vdd_ana;
static struct regulator *vcc_i2c;

/**
 * gt1x_parse_dt - parse platform infomation form devices tree.
 */
static int gt1x_parse_dt(struct device *dev)
{
	struct device_node *np;
    int ret = 0;

    if (!dev)
        return -ENODEV;

    np = dev->of_node;
	gt1x_int_gpio = of_get_named_gpio(np, "goodix,irq-gpio", 0);
	gt1x_rst_gpio = of_get_named_gpio(np, "goodix,reset-gpio", 0);

    if (!gpio_is_valid(gt1x_int_gpio) || !gpio_is_valid(gt1x_rst_gpio)) {
        GTP_ERROR("Invalid GPIO, irq-gpio:%d, rst-gpio:%d",
            gt1x_int_gpio, gt1x_rst_gpio);
        return -EINVAL;
    }

    vdd_ana = regulator_get(dev, "vdd_ana");
    if (IS_ERR(vdd_ana)) {
	    GTP_ERROR("regulator get of vdd_ana failed");
	    ret = PTR_ERR(vdd_ana);
	    vdd_ana = NULL;
	    return ret;
    }

	vcc_i2c = regulator_get(dev, "vcc_i2c");
	if (IS_ERR(vcc_i2c)) {
		GTP_ERROR("regulator get of vcc_i2c failed");
		ret = PTR_ERR(vcc_i2c);
		vcc_i2c = NULL;
		goto ERR_GET_VCC;
	}
    return 0;
ERR_GET_VCC:
    regulator_put(vdd_ana);
    vdd_ana = NULL;
    return ret;

}

/**
 * goodix_pinctrl_init - pinctrl init
 */
static int goodix_pinctrl_init(struct i2c_client *client)
{
	int ret = 0;

	gt_pinctrl.ts_pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(gt_pinctrl.ts_pinctrl)) {
		GTP_ERROR("Failed to get pinctrl");
		ret = PTR_ERR(gt_pinctrl.ts_pinctrl);
		gt_pinctrl.ts_pinctrl= NULL;
		return ret;
	}

	gt_pinctrl.int_default = pinctrl_lookup_state(gt_pinctrl.ts_pinctrl, "default");
	if (IS_ERR_OR_NULL(gt_pinctrl.int_default)) {
		GTP_ERROR("Pin state[default] not found");
		ret = PTR_ERR(gt_pinctrl.int_default);
		goto exit_put;
	}

	gt_pinctrl.int_out_high = pinctrl_lookup_state(gt_pinctrl.ts_pinctrl, "int-output-high");
	if (IS_ERR_OR_NULL(gt_pinctrl.int_out_high)) {
		GTP_ERROR("Pin state[int-output-high] not found");
		ret = PTR_ERR(gt_pinctrl.int_out_high);
	}

	gt_pinctrl.int_out_low = pinctrl_lookup_state(gt_pinctrl.ts_pinctrl, "int-output-low");
	if (IS_ERR_OR_NULL(gt_pinctrl.int_out_low)) {
		GTP_ERROR("Pin state[int-output-low] not found");
		ret = PTR_ERR(gt_pinctrl.int_out_low);
		goto exit_put;
	}

	gt_pinctrl.int_input = pinctrl_lookup_state(gt_pinctrl.ts_pinctrl, "int-input");
	if (IS_ERR_OR_NULL(gt_pinctrl.int_input)) {
		GTP_ERROR("Pin state[int-input] not found");
		ret = PTR_ERR(gt_pinctrl.int_input);
		goto exit_put;
	}
	gt_pinctrl.erst_as_default = pinctrl_lookup_state(gt_pinctrl.ts_pinctrl, "erst_as_default");
	if (IS_ERR_OR_NULL(gt_pinctrl.erst_as_default)) {
		GTP_ERROR("Pin state[int-output-high] not found");
		ret = PTR_ERR(gt_pinctrl.erst_as_default);
	}

	gt_pinctrl.erst_output_high = pinctrl_lookup_state(gt_pinctrl.ts_pinctrl, "erst_output_high");
	if (IS_ERR_OR_NULL(gt_pinctrl.erst_output_high)) {
		GTP_ERROR("Pin state[int-output-low] not found");
		ret = PTR_ERR(gt_pinctrl.erst_output_high);
		goto exit_put;
	}

	gt_pinctrl.erst_output_low = pinctrl_lookup_state(gt_pinctrl.ts_pinctrl, "erst_output_low");
	if (IS_ERR_OR_NULL(gt_pinctrl.erst_output_low)) {
		GTP_ERROR("Pin state[int-input] not found");
		ret = PTR_ERR(gt_pinctrl.erst_output_low);
		goto exit_put;
	}
	return 0;
exit_put:
	devm_pinctrl_put(gt_pinctrl.ts_pinctrl);
	gt_pinctrl.ts_pinctrl= NULL;
	gt_pinctrl.int_default = NULL;
	gt_pinctrl.int_out_high = NULL;
	gt_pinctrl.int_out_low = NULL;
	gt_pinctrl.int_input= NULL;
	gt_pinctrl.erst_as_default= NULL;
	gt_pinctrl.erst_output_high = NULL;
	gt_pinctrl.erst_output_low = NULL;

	return ret;
}

/**
 * gt1x_power_switch - power switch .
 * @on: 1-switch on, 0-switch off.
 * return: 0-succeed, -1-faileds
 */
int gt1x_power_switch(int on)
{

	int ret;
	struct i2c_client *client = gt1x_i2c_client;

    if (!client || !vdd_ana || !vcc_i2c)
        return -EPERM;
    pinctrl_select_state(gt_pinctrl.ts_pinctrl,gt_pinctrl.erst_output_low);
	pinctrl_select_state(gt_pinctrl.ts_pinctrl,gt_pinctrl.int_out_low);

	if (on) {
		GTP_DEBUG("GTP power on.");
		ret = regulator_enable(vdd_ana);
		udelay(2);
		ret = regulator_enable(vcc_i2c);
	} else {
		GTP_DEBUG("GTP power off.");
		ret = regulator_disable(vcc_i2c);
		udelay(2);
		ret = regulator_disable(vdd_ana);
	}
	return ret;

}
#endif

static void gt1x_remove_gpio_and_power(void)
{
    if (gpio_is_valid(gt1x_int_gpio))
        gpio_free(gt1x_int_gpio);

    if (gpio_is_valid(gt1x_rst_gpio))
        gpio_free(gt1x_rst_gpio);

#ifdef GTP_CONFIG_OF
    if (vcc_i2c)
        regulator_put(vcc_i2c);

    if (vdd_ana)
        regulator_put(vdd_ana);
#endif

    if (gt1x_i2c_client && gt1x_i2c_client->irq)
        free_irq(gt1x_i2c_client->irq, gt1x_i2c_client);

}


/**
 * gt1x_request_io_port - Request gpio(INT & RST) ports.
 */
static s32 gt1x_request_io_port(void)
{
	s32 ret = 0;

	GTP_DEBUG_FUNC();
	ret = gpio_request(GTP_INT_PORT, "GTP_INT_IRQ");
	if (ret < 0) {
		GTP_ERROR("Failed to request GPIO:%d, ERRNO:%d", (s32) GTP_INT_PORT, ret);
		ret = -ENODEV;
	} else {
		GTP_GPIO_AS_INT(GTP_INT_PORT);
		gt1x_i2c_client->irq = GTP_INT_IRQ;
	}

	ret = gpio_request(GTP_RST_PORT, "GTP_RST_PORT");
	if (ret < 0) {
		GTP_ERROR("Failed to request GPIO:%d, ERRNO:%d", (s32) GTP_RST_PORT, ret);
		ret = -ENODEV;
	}


	if (ret < 0) {
		gpio_free(GTP_RST_PORT);
		gpio_free(GTP_INT_PORT);
	}

	return ret;
}

/**
 * gt1x_request_irq - Request interrupt.
 * Return
 *      0: succeed, -1: failed.
 */

static s32 gt1x_request_irq(void)
{
	s32 ret = -1;
	const u8 irq_table[] = GTP_IRQ_TAB;

	GTP_DEBUG_FUNC();
	GTP_DEBUG("INT trigger type:%x", gt1x_int_type);

	ret = request_irq(gt1x_i2c_client->irq, gt1x_ts_irq_handler, irq_table[gt1x_int_type], gt1x_i2c_client->name, gt1x_i2c_client);
	if (ret) {
		GTP_ERROR("Request IRQ failed!ERRNO:%d.", ret);
		GTP_GPIO_AS_INPUT(GTP_INT_PORT);
		gpio_free(GTP_INT_PORT);

		return -EPERM;
	} else {
		gt1x_irq_disable();
		return 0;
	}
}

/**
 * gt1x_request_input_dev -  Request input device Function.
 * Return
 *      0: succeed, -1: failed.
 */
static s8 gt1x_request_input_dev(void)
{
	s8 ret = -1;
#if GTP_HAVE_TOUCH_KEY
	u8 index = 0;
#endif

	GTP_DEBUG_FUNC();

	input_dev = input_allocate_device();
	if (input_dev == NULL) {
		GTP_ERROR("Failed to allocate input device.");
		return -ENOMEM;
	}

	input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
#if GTP_ICS_SLOT_REPORT
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 7, 0))
    input_mt_init_slots(input_dev, GTP_MAX_TOUCH, INPUT_MT_DIRECT);
#else
    input_mt_init_slots(input_dev, GTP_MAX_TOUCH);
#endif
#endif
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

#if GTP_HAVE_TOUCH_KEY
	for (index = 0; index < GTP_MAX_KEY_NUM; index++) {
		input_set_capability(input_dev, EV_KEY, gt1x_touch_key_array[index]);
	}
#endif

#if GTP_GESTURE_WAKEUP
	input_dev->event = gt1x_select_gesture_mode;
	input_set_capability(input_dev, EV_KEY, KEY_GES_REGULAR);
    input_set_capability(input_dev, EV_KEY, KEY_GES_CUSTOM);

    input_set_capability(input_dev, EV_KEY, KEY_WAKEUP);

#endif

#if GTP_CHANGE_X2Y
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, gt1x_abs_y_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, gt1x_abs_x_max, 0, 0);
#else
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, gt1x_abs_x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, gt1x_abs_y_max, 0, 0);
#endif
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

	input_dev->name = gt1x_ts_name;
	input_dev->phys = input_dev_phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->id.vendor = 0xDEAD;
	input_dev->id.product = 0xBEEF;
	input_dev->id.version = 10427;

	ret = input_register_device(input_dev);
	if (ret) {
		GTP_ERROR("Register %s input device failed", input_dev->name);
		return -ENODEV;
	}

	return 0;
}



/**
 * gt1x_ts_probe -   I2c nn.
 * @client: i2c device struct.
 * @id: device id.
 * Return  0: succeed, -1: failed.
 */
static int gt1x_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	s32 ret = -1;
#if GTP_AUTO_UPDATE
	struct task_struct *thread = NULL;
#endif

/*shenwenbin add for show TP info for *#87# 20180120 begin*/
#ifdef SUPPORT_READ_TP_VERSION
	u32 version_info;
	char tp_version[50] = {0};
	u8 Config_Version = 0;
#endif
/*shenwenbin add for show TP info for *#87# 20180120 end*/


	GTP_INFO("GTP Driver Version: %s", GTP_DRIVER_VERSION);
	GTP_INFO("GTP I2C Address: 0x%02x", client->addr);
	gt1x_i2c_client = client;
	spin_lock_init(&irq_lock);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		GTP_ERROR("I2C check functionality failed.");
		return -ENODEV;
	}

#ifdef GTP_CONFIG_OF	/* device tree support */
	if (client->dev.of_node) {
		gt1x_parse_dt(&client->dev);
	}
#endif
	/* init pinctrl states */
	ret  = goodix_pinctrl_init(client);
	if (ret < 0)
		GTP_ERROR("Init pinctrl states failed.");



    if (!ret && gt_pinctrl.ts_pinctrl) {
        ret = pinctrl_select_state(gt_pinctrl.ts_pinctrl, gt_pinctrl.int_out_low);
        if (ret < 0) {
            dev_err(&client->dev,"failed to select pin to eint_output_low");
        }
        ret = pinctrl_select_state(gt_pinctrl.ts_pinctrl,gt_pinctrl.erst_output_low);
        if (ret < 0) {
            dev_err(&client->dev,"failed to select pin to erst_as_default");
        }
        msleep(10);
    }
    ret = gt1x_power_switch(1);
    if (ret) {
        GTP_ERROR("GTP power on failed.");
        return -EINVAL;
    }
    msleep(50);






	ret = gt1x_request_io_port();
	if (ret < 0) {
		GTP_ERROR("GTP request IO port failed.");
		return ret;
	}

	ret = gt1x_init();
    if (ret != 0)
		  goto ERROR_INIT;

	INIT_WORK(&gt1x_work, gt1x_ts_work_func);

	ret = gt1x_request_input_dev();
	if (ret < 0) {
		GTP_ERROR("GTP request input dev failed");
	}

	ret = gt1x_request_irq();
	if (ret < 0) {
		GTP_INFO("GTP works in polling mode.");
	} else {
		GTP_INFO("GTP works in interrupt mode.");
	}


    get_lockdown_info(client,gt1x_tp_lockdown_info);



#if GTP_GESTURE_WAKEUP
	enable_irq_wake(client->irq);
#endif

	gt1x_irq_enable();

#if GTP_ESD_PROTECT

	gt1x_init_esd_protect();
	gt1x_esd_switch(SWITCH_ON);
#endif

/*shenwenbin add for show TP info for *#87# 20180120 begin*/
#ifdef SUPPORT_READ_TP_VERSION
	ret = gt1x_i2c_read(GTP_REG_CONFIG_DATA, &Config_Version, 1);
	if (ret < 0)
	{
		GTP_ERROR("Read config version failed.");
	}
	version_info = gt1x_version.patch_id;
	memset(tp_version, 0, sizeof(tp_version));
	sprintf(tp_version, "[FW]0x%06X,[IC]GT1151Q,Config_Version:%d",version_info,Config_Version);
	init_tp_fm_info(0, tp_version, "truly");
#endif
/*shenwenbin add for show TP info for *#87# 20180120 end*/

#if GTP_AUTO_UPDATE
	thread = kthread_run(gt1x_auto_update_proc, (void *)NULL, "gt1x_auto_update");
	if (IS_ERR(thread)) {
		ret = PTR_ERR(thread);
		GTP_ERROR("Failed to create auto-update thread: %d.", ret);
	}
#endif

	gt1x_register_powermanger();

    goodix_create_sysfs(client);


 /*shenwenbin add for runin 20180120 begin*/

 /*shenwenbin add for runin 20180120 end*/
  /* Begin xiaopei add gesture wakeup 2018-3-9 */
  tp_gesture_creat_sys_entry();
  /* End xiaopei add gesture wakeup 2018-3-9 */


#if  GTP_ITO_TEST_SELF
	gtp_test_sysfs_init();
#endif

INIT_WORK(&gt_resmue, gt1x_resume);
INIT_WORK(&gt_suspend, gt1x_suspend);


	return 0;

ERROR_INIT:
    gpio_free(GTP_RST_PORT);
	gpio_free(GTP_INT_PORT);
	devm_pinctrl_put(gt_pinctrl.ts_pinctrl);
	regulator_disable(vdd_ana);
	regulator_disable(vcc_i2c);
/*shenwenbin add for runin 20180120 begin*/

/*shenwenbin add for runin 20180120 end*/

	return ret;




}

/**
 * gt1x_ts_remove -  Goodix touchscreen driver release function.
 * @client: i2c device struct.
 * Return  0: succeed, -1: failed.
 */
static int gt1x_ts_remove(struct i2c_client *client)
{
	GTP_DEBUG_FUNC();
	GTP_INFO("GTP driver removing...");
	gt1x_unregister_powermanger();

#if GTP_GESTURE_WAKEUP
	disable_irq_wake(client->irq);
#endif
    gt1x_deinit();
	input_unregister_device(input_dev);
    gt1x_remove_gpio_and_power();

        goodix_remove_sysfs(client);



    return 0;
}

#if   defined(CONFIG_FB)

/* frame buffer notifier block control the suspend/resume procedure */
static struct notifier_block gt1x_fb_notifier;

static int gtp_fb_notifier_callback(struct notifier_block *noti, unsigned long event, void *data)
{
	struct fb_event *ev_data = data;
	int *blank;

#if GTP_INCELL_PANEL
    #ifndef FB_EARLY_EVENT_BLANK
        #error Need add FB_EARLY_EVENT_BLANK to fbmem.c
    #endif

	if (ev_data && ev_data->data && event == FB_EARLY_EVENT_BLANK) {
		blank = ev_data->data;

        if (*blank == FB_BLANK_UNBLANK) {

			GTP_DEBUG("Resume by fb notifier.");
			gt1x_resume();
        }
    }
#else
	if (ev_data && ev_data->data && event == FB_EVENT_BLANK) {
		blank = ev_data->data;
        if (*blank == FB_BLANK_UNBLANK) {
			GTP_DEBUG("Resume by fb notifier.");

			queue_work(gt1x_wq, &gt_resmue);

        }
    }
#endif

	if (ev_data && ev_data->data && event == FB_EVENT_BLANK) {
		blank = ev_data->data;

        if (*blank == FB_BLANK_POWERDOWN) {

			/* begin xiaopei modify goodix can not sleep ,when suspend 2018-3-2*/

			/* End xiaopei modify goodix can not sleep ,when suspend 2018-3-2*/
			queue_work(gt1x_wq, &gt_suspend);
			GTP_DEBUG("Suspend by mdss_dsi.c");
		}
	}

	return 0;
}

#elif defined(CONFIG_PM)
/**
 * gt1x_ts_suspend - i2c suspend callback function.
 * @dev: i2c device.
 * Return  0: succeed, -1: failed.
 */

static int gt1x_pm_suspend(struct device *dev)
{

	return gt1x_suspend();
}

/**
 * gt1x_ts_resume - i2c resume callback function.
 * @dev: i2c device.
 * Return  0: succeed, -1: failed.
 */
static int gt1x_pm_resume(struct device *dev)
{
	return gt1x_resume();
}

/* bus control the suspend/resume procedure */
static const struct dev_pm_ops gt1x_ts_pm_ops = {
	.suspend = gt1x_pm_suspend,
	.resume = gt1x_pm_resume,
};

#elif defined(CONFIG_HAS_EARLYSUSPEND)
/* earlysuspend module the suspend/resume procedure */
static void gt1x_ts_early_suspend(struct early_suspend *h)
{
	gt1x_suspend();
}

static void gt1x_ts_late_resume(struct early_suspend *h)
{
	gt1x_resume();
}

static struct early_suspend gt1x_early_suspend = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
	.suspend = gt1x_ts_early_suspend,
	.resume = gt1x_ts_late_resume,
};
#endif


static int gt1x_register_powermanger(void)
{
#if   defined(CONFIG_FB)
	gt1x_fb_notifier.notifier_call = gtp_fb_notifier_callback;
	fb_register_client(&gt1x_fb_notifier);

#elif defined(CONFIG_HAS_EARLYSUSPEND)
	register_early_suspend(&gt1x_early_suspend);
#endif
	return 0;
}

static int gt1x_unregister_powermanger(void)
{
#if   defined(CONFIG_FB)
	fb_unregister_client(&gt1x_fb_notifier);

#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&gt1x_early_suspend);
#endif
	return 0;
}

#ifdef GTP_CONFIG_OF
static const struct of_device_id gt1x_match_table[] = {
		{.compatible = "goodix,gt1x",},
		{ },
};
#endif

static const struct i2c_device_id gt1x_ts_id[] = {
	{GTP_I2C_NAME, 0},
	{}
};

static struct i2c_driver gt1x_ts_driver = {
	.probe = gt1x_ts_probe,
	.remove = gt1x_ts_remove,
	.id_table = gt1x_ts_id,
	.driver = {
		   .name = GTP_I2C_NAME,
		   .owner = THIS_MODULE,
#ifdef GTP_CONFIG_OF
		   .of_match_table = gt1x_match_table,
#endif
#if !defined(CONFIG_FB) && defined(CONFIG_PM)
		   .pm = &gt1x_ts_pm_ops,
#endif
		   },
};

/**
 * gt1x_ts_init - Driver Install function.
 * Return   0---succeed.
 */
static int __init gt1x_ts_init(void)
{
	int ret = 0;
	GTP_DEBUG_FUNC();
	/* add verify LCD by qujiong start */
	if (IS_ERR_OR_NULL(g_lcd_id))
	{
		GTP_ERROR("g_lcd_id is ERROR!\n");
		ret = -ENODEV;
		return ret;
	}
	else
	{
		if (strstr(g_lcd_id,"CSOT otm1901a") != NULL)
		{
			GTP_INFO("LCM is right! [Vendor]CSOT [IC] otm1901a\n");
		}
		else
		{
			GTP_ERROR("Unknown LCM!\n");
			ret = -ENODEV;
			return ret;
		}
	}
	/* add verify LCD by qujiong end */

	GTP_INFO("GTP driver installing...");
	gt1x_wq = create_singlethread_workqueue("gt1x_wq");
	if (!gt1x_wq) {
		GTP_ERROR("Creat workqueue failed.");
		return -ENOMEM;
	}

	return i2c_add_driver(&gt1x_ts_driver);
}

/**
 * gt1x_ts_exit - Driver uninstall function.
 * Return   0---succeed.
 */
static void __exit gt1x_ts_exit(void)
{
	GTP_DEBUG_FUNC();
	GTP_INFO("GTP driver exited.");
	i2c_del_driver(&gt1x_ts_driver);
	if (gt1x_wq) {
		destroy_workqueue(gt1x_wq);
	}
}

module_init(gt1x_ts_init);
module_exit(gt1x_ts_exit);

MODULE_DESCRIPTION("GTP Series Driver");
MODULE_LICENSE("GPL");
