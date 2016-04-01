
#include "lct_ctp_gesture.h"

static struct device ctp_gesture_dev = {
#if defined(CONFIG_CT820_COMMON)
	.init_name = "mx_tsp",
#else
	.init_name = "ctp_gesture",
#endif
};
void ctp_gesture_set_input_device(struct input_dev *input)
{
	struct input_dev *input_device = input;
	struct ctp_gesture_device *pdata = dev_get_drvdata(&ctp_gesture_dev);

	printk("%s\n", __func__);
	pdata->input_device = input_device;
}
EXPORT_SYMBOL_GPL(ctp_gesture_set_input_device);

int ctp_get_gesture_data(void)
{
	struct ctp_gesture_device *pdata = dev_get_drvdata(&ctp_gesture_dev);

	pr_debug("%s, current_gesture_val=0x%08x\n", __func__, pdata->current_gesture_val);
	return pdata->current_gesture_val;
}
EXPORT_SYMBOL_GPL(ctp_get_gesture_data);

void ctp_set_gesture_data(int value)
{
	struct ctp_gesture_device *pdata = dev_get_drvdata(&ctp_gesture_dev);
	int val = value;

	pr_debug("%s, val=0x%08x\n", __func__, val);
	pdata->current_gesture_val = val;
}
EXPORT_SYMBOL_GPL(ctp_set_gesture_data);

static ssize_t ctp_gesture_data_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ctp_gesture_device *pdata = dev_get_drvdata(dev);
	int ret = 0;

	pr_debug("%s, current_gesture_val=0x%08x\n", __func__, pdata->current_gesture_val);

	mutex_lock(&pdata->gesture_lock);
	ret = sprintf(buf, "%d\n", pdata->current_gesture_val);
	mutex_unlock(&pdata->gesture_lock);

	pdata->current_gesture_val = 0;

	return ret;
}

static ssize_t ctp_gesture_data_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct ctp_gesture_device *pdata = dev_get_drvdata(dev);
	unsigned long value;
	ssize_t ret = -EINVAL;

#if defined(CONFIG_CT820_COMMON)
	if (!buf)
		return ret;
	else
		value = (int)*buf;

	pr_debug("%s, value=0x%08lx\n", __func__, value);
#else
	ret = kstrtoul(buf, 8, &value);
	if (ret)
		return ret;

	pr_debug("%s, buf=%s, value=%ld\n", __func__, buf, value);
#endif

	mutex_lock(&pdata->gesture_lock);
	pdata->current_gesture_val = value;
	mutex_unlock(&pdata->gesture_lock);

	return count;
}

static DEVICE_ATTR(gesture_data, 0660, ctp_gesture_data_show, ctp_gesture_data_store);

int ctp_get_gesture_control(void)
{
	struct ctp_gesture_device *pdata = dev_get_drvdata(&ctp_gesture_dev);

	printk("%s, control_state=0x%08x\n", __func__, pdata->control_state);
	return pdata->control_state & BIT_GESTURE_ALL;
}
EXPORT_SYMBOL_GPL(ctp_get_gesture_control);

void ctp_set_gesture_control(int value)
{
	struct ctp_gesture_device *pdata = dev_get_drvdata(&ctp_gesture_dev);
	int val = value;

	printk("%s, val=0x%08x\n", __func__, val);
	pdata->control_state = val;
}
EXPORT_SYMBOL_GPL(ctp_set_gesture_control);

static Func_Ctp_Gesture_Mode Func_ctp_gesture_switch;

void ctp_gesture_switch_init(Func_Ctp_Gesture_Mode gesture_func)
{
	Func_ctp_gesture_switch = gesture_func;
}

static int ctp_gesture_mode_switch(int enable)
{
	int val = enable;

	if (Func_ctp_gesture_switch) {
		printk("%s, enable\n", __func__);
		return Func_ctp_gesture_switch(val);
	} else {
		printk("%s, no function\n", __func__);
		return 0;
	}
}

static ssize_t ctp_gesture_control_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ctp_gesture_device *pdata = dev_get_drvdata(dev);
	int ret = 0;

	pr_debug("%s, control_state=0x%08x\n", __func__, pdata->control_state);

	mutex_lock(&pdata->gesture_lock);
	ret = sprintf(buf, "0x%08x\n", pdata->control_state);
	mutex_unlock(&pdata->gesture_lock);

	return ret;
}

static ssize_t ctp_gesture_control_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct ctp_gesture_device *pdata = dev_get_drvdata(dev);
	unsigned long value;
	ssize_t ret = -EINVAL;

#if defined(CONFIG_CT820_COMMON)
	if (!buf)
		return ret;
	else
		value = buf[0]|(buf[1]<<8)|(buf[2]<<16)|(buf[3]<<24);

	printk("%s, value=0x%08lx, 0x%02x,0x%02x,0x%02x,0x%02x, 0x%08x\n", __func__,
		value, buf[0], buf[1], buf[2], buf[3], buf[4]);
#else
	ret = kstrtoul(buf, 8, &value);
	if (ret)
		return ret;

	pr_debug("%s, buf=%s, value=%ld\n", __func__, buf, value);
#endif

	mutex_lock(&pdata->gesture_lock);

	pdata->current_control_value = value;

	if (value == CONTROL_AUTOTEST_ENTER) {
		pr_debug("%s, CONTROL_AUTOTEST_ENTER\n", __func__);
		pdata->control_state |= (BIT_GESTURE_ALL|BIT_STATUS_ALL);
		ctp_gesture_mode_switch(1);
	} else if (value == CONTROL_AUTOTEST_EXIT) {
		pr_debug("%s, CONTROL_AUTOTEST_EXIT\n", __func__);
		pdata->control_state &= BIT_STATUS_OFF;
		ctp_gesture_mode_switch(0);
	} else if ((value & CONTROL_SWIPE) &&
		(!(value & ~CONTROL_SWIPE) || (value & CONTROL_SWIPE_ALL))) {
		pr_debug("%s, CONTROL_SWIPE\n", __func__);
		if (value & CONTROL_SWIPE_RIGHT_ENABLE)
			pdata->control_state |= BIT_SWIPE_RIGHT;
		else
			pdata->control_state &= ~BIT_SWIPE_RIGHT;
		if (value & CONTROL_SWIPE_LEFT_ENABLE)
			pdata->control_state |= BIT_SWIPE_LEFT;
		else
			pdata->control_state &= ~BIT_SWIPE_LEFT;
		if (value & CONTROL_SWIPE_DOWN_ENABLE)
			pdata->control_state |= BIT_SWIPE_DOWN;
		else
			pdata->control_state &= ~BIT_SWIPE_DOWN;
		if (value & CONTROL_SWIPE_UP_ENABLE)
			pdata->control_state |= BIT_SWIPE_UP;
		else
			pdata->control_state &= ~BIT_SWIPE_UP;

		pdata->control_state_bakup = 0;
	} else if (((value & CONTROL_UNICODE) == CONTROL_UNICODE) &&
		(!(value & ~CONTROL_UNICODE) || (value & CONTROL_UNICODE_ALL))) {
		pr_debug("%s, CONTROL_UNICODE\n", __func__);
		if (value & CONTROL_UNICODE_V_ENABLE)
			pdata->control_state |= BIT_UNICODE_V;
		else
			pdata->control_state &= ~BIT_UNICODE_V;
		if (value & CONTROL_UNICODE_C_ENABLE)
			pdata->control_state |= BIT_UNICODE_C;
		else
			pdata->control_state &= ~BIT_UNICODE_C;
		if (value & CONTROL_UNICODE_E_ENABLE)
			pdata->control_state |= BIT_UNICODE_E;
		else
			pdata->control_state &= ~BIT_UNICODE_E;
		if (value & CONTROL_UNICODE_W_ENABLE)
			pdata->control_state |= BIT_UNICODE_W;
		else
			pdata->control_state &= ~BIT_UNICODE_W;
		if (value & CONTROL_UNICODE_M_ENABLE)
			pdata->control_state |= BIT_UNICODE_M;
		else
			pdata->control_state &= ~BIT_UNICODE_M;
		if (value & CONTROL_UNICODE_S_ENABLE)
			pdata->control_state |= BIT_UNICODE_S;
		else
			pdata->control_state &= ~BIT_UNICODE_S;
		if (value & CONTROL_UNICODE_Z_ENABLE)
			pdata->control_state |= BIT_UNICODE_Z;
		else
			pdata->control_state &= ~BIT_UNICODE_Z;
		if (value & CONTROL_UNICODE_O_ENABLE)
			pdata->control_state |= BIT_UNICODE_O;
		else
			pdata->control_state &= ~BIT_UNICODE_O;

		pdata->control_state_bakup = 0;
	} else if ((value & CONTROL_TAP) &&
		(!(value & ~CONTROL_TAP) || (value & CONTROL_TAP_ENABLE))) {
		pr_debug("%s, CONTROL_TAP\n", __func__);
		if (value & CONTROL_TAP_ENABLE)
			pdata->control_state |= BIT_DOUBLE_TAP;
		else
			pdata->control_state &= ~BIT_DOUBLE_TAP;

		pdata->control_state_bakup = 0;
	} else if ((value & CONTROL_ALL) &&
		(!(value & ~(CONTROL_ALL)) ||
		!(value & ~(CONTROL_ALL|CONTROL_ALL_ENABLE)))) {
		pr_debug("%s, CONTROL_ALL, control_state_bakup=0x%08x\n", __func__, pdata->control_state_bakup);
		if (value & CONTROL_ALL_ENABLE) {
			if (pdata->control_state_bakup)
				pdata->control_state = pdata->control_state_bakup;
		} else {
			pdata->control_state_bakup = pdata->control_state;
			pdata->control_state &= BIT_STATUS_OFF;
		}
	}

	pr_debug("%s, control_state=0x%08x\n", __func__, pdata->control_state);
	if (!(pdata->control_state & BIT_STATUS_ALL) && (pdata->control_state & BIT_GESTURE_ALL)) {
		pdata->control_state |= BIT_STATUS_PART;
	}
	pr_debug("%s, last control_state=0x%08x\n", __func__, pdata->control_state);

	mutex_unlock(&pdata->gesture_lock);

	return count;
}

static DEVICE_ATTR(gesture_control, 0660, ctp_gesture_control_show, ctp_gesture_control_store);

bool ctp_check_gesture_needed(u8 report_data)
{
	struct ctp_gesture_device *pdata = dev_get_drvdata(&ctp_gesture_dev);
	u8 key_data = report_data;
	u32 key_mask = 0;

	pr_debug("%s, key_data=0x%02x\n", __func__, key_data);

	switch (key_data) {
	case DOUBLE_TAP:
		key_mask = BIT_DOUBLE_TAP;
		break;
	case SWIPE_X_LEFT:
		key_mask = BIT_SWIPE_LEFT;
		break;
	case SWIPE_X_RIGHT:
		key_mask = BIT_SWIPE_RIGHT;
		break;
	case SWIPE_Y_UP:
		key_mask = BIT_SWIPE_UP;
		break;
	case SWIPE_Y_DOWN:
		key_mask = BIT_SWIPE_DOWN;
		break;
	case UNICODE_E:
		key_mask = BIT_UNICODE_E;
		break;
	case UNICODE_C:
		key_mask = BIT_UNICODE_C;
		break;
	case UNICODE_W:
		key_mask = BIT_UNICODE_W;
		break;
	case UNICODE_M:
		key_mask = BIT_UNICODE_M;
		break;
	case UNICODE_O:
		key_mask = BIT_UNICODE_O;
		break;
	case UNICODE_S:
		key_mask = BIT_UNICODE_S;
		break;
	case UNICODE_V_DOWN:
		key_mask = BIT_UNICODE_V;
		break;
	case UNICODE_Z:
		key_mask = BIT_UNICODE_Z;
		break;
	}

	return pdata->control_state & key_mask;

}

#if defined(CONFIG_TOUCHSCREEN_COVER)

static Func_Ctp_Cover_State Func_ctp_cover_state;

void ctp_cover_switch_init(Func_Ctp_Cover_State cover_func)
{
	Func_ctp_cover_state = cover_func;
}

static int ctp_cover_state_switch(int enable)
{
	int val = enable;

	if (Func_ctp_cover_state) {
		printk("%s, enable\n", __func__);
		return Func_ctp_cover_state(val);
	} else {
		printk("%s, no function\n", __func__);
		return 0;
	}
}

static ssize_t ctp_cover_data_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ctp_gesture_device *pdata = dev_get_drvdata(dev);
	int ret = 0;

	pr_debug("%s, cover_state=0x%08x\n", __func__, pdata->cover_state);

	mutex_lock(&pdata->cover_lock);
	ret = sprintf(buf, "%d\n", pdata->cover_state);
	mutex_unlock(&pdata->cover_lock);

	return ret;
}

static ssize_t ctp_cover_data_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct ctp_gesture_device *pdata = dev_get_drvdata(dev);
	unsigned long value;
	ssize_t ret = -EINVAL;

#if defined(CONFIG_CT820_COMMON)
	if (!buf)
		return ret;
	else
		value = (int)*buf;

	pr_debug("%s, value=0x%08lx\n", __func__, value);
#else
	ret = kstrtoul(buf, 8, &value);
	if (ret)
		return ret;

	pr_debug("%s, buf=%s, value=%ld\n", __func__, buf, value);
#endif

	mutex_lock(&pdata->cover_lock);
	pdata->cover_state = value;
	mutex_unlock(&pdata->cover_lock);

	ret = ctp_cover_state_switch(value);
	if (ret >= 0)
		printk("%s, ret=%d\n", __func__, ret);
	return count;
}

static DEVICE_ATTR(cover_control, 0660, ctp_cover_data_show, ctp_cover_data_store);
#endif

static int __init ctp_gesture_init(void)
{
	struct ctp_gesture_device *pdata = NULL;
	int ret;

	printk("%s\n", __func__);
	ret = device_register(&ctp_gesture_dev);
	if (ret) {
		pr_err("Error registering ctp gesture device\n");
		goto Err_device_register;
	}

	pdata = devm_kzalloc(&ctp_gesture_dev, sizeof(struct ctp_gesture_device), GFP_KERNEL);
	if (!pdata) {
		dev_err(&ctp_gesture_dev, "Failed to allocated sn3191_led_pdata\n");
		goto Err_kzaloc;
	}

	pdata->gesture_dev = ctp_gesture_dev;
	dev_set_drvdata(&ctp_gesture_dev, pdata);
	printk("%s, control_state=%d, 0x%08x\n", __func__, pdata->control_state, pdata->control_state);

	mutex_init(&pdata->gesture_lock);

	ret = sysfs_create_file(&ctp_gesture_dev.kobj, &dev_attr_gesture_data.attr);
	if (ret) {
		pr_err("Error creating ctp gesture sysfs file\n");
		goto Err_sysfs_create_data;
	}

	ret = sysfs_create_file(&ctp_gesture_dev.kobj, &dev_attr_gesture_control.attr);
	if (ret) {
		pr_err("Error creating ctp gesture sysfs file\n");
		goto Err_sysfs_create_control;
	}

#if defined(CONFIG_TOUCHSCREEN_COVER)
	mutex_init(&pdata->cover_lock);

	ret = sysfs_create_file(&ctp_gesture_dev.kobj, &dev_attr_cover_control.attr);
	if (ret) {
		pr_err("Error creating ctp gesture sysfs file\n");
		goto Err_sysfs_create_control;
	}
#endif

	printk("%s done\n", __func__);
	return 0;

Err_sysfs_create_control:
	sysfs_remove_file(&ctp_gesture_dev.kobj, &dev_attr_gesture_data.attr);
Err_sysfs_create_data:
	devm_kfree(&ctp_gesture_dev, pdata);
	device_unregister(&ctp_gesture_dev);
Err_kzaloc:
Err_device_register:
	printk ("%s error, ret=%d\n", __func__, ret);
	return ret;
}

static void __exit ctp_gesture_exit(void)
{
	struct ctp_gesture_device *pdata = container_of(&ctp_gesture_dev, struct ctp_gesture_device, gesture_dev);

#if defined(CONFIG_TOUCHSCREEN_COVER)
	sysfs_remove_file(&ctp_gesture_dev.kobj, &dev_attr_cover_control.attr);
	mutex_destroy(&pdata->cover_lock);
#endif
	sysfs_remove_file(&ctp_gesture_dev.kobj, &dev_attr_gesture_control.attr);
	sysfs_remove_file(&ctp_gesture_dev.kobj, &dev_attr_gesture_data.attr);
	mutex_destroy(&pdata->gesture_lock);
	devm_kfree(&ctp_gesture_dev, pdata);
	device_unregister(&ctp_gesture_dev);
}

module_init(ctp_gesture_init);
module_exit(ctp_gesture_exit);

MODULE_DESCRIPTION("CTP gesture driver");
MODULE_LICENSE("GPL");


