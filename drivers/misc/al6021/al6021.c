/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/clk.h>
#include <linux/irqreturn.h>
#include "al6021.h"


////////////////////////////////////////////////////////////////////////////////
static u32 diff_in_us(struct timespec64 t1, struct timespec64 t2)
{
	struct timespec64 diff;

       diff.tv_sec  = t2.tv_sec - t1.tv_sec;
       diff.tv_nsec = t2.tv_nsec - t1.tv_nsec;

	return ((u32)(diff.tv_sec * 1000000 + diff.tv_nsec/1000));
}

static int write_reset_pin(struct al6021_data_info *data, u32 value)
{
	int result = -1;

	if (data->reset_gpio > 0) {
		result = gpio_direction_output(data->reset_gpio, value);
		pr_i("write_reset_pin to %d, result = %d", value, result);
	}

	return result;
}

static int change_spi_speed(struct al6021_data_info *data, u32 value)
{
	int result;
	u32 tmp;

	tmp = data->spi->max_speed_hz;

	if (value)
		data->spi->max_speed_hz = value;
	else
		data->spi->max_speed_hz = AL6021_SPI_SPEED_HZ;

	result = spi_setup(data->spi);

	if (result) {
		data->spi->max_speed_hz = tmp;
	}
	pr_i("spi_setup to %d, result = %d, current speed is %d",
		value, result, data->spi->max_speed_hz);

	return result;
}

static ssize_t al6021_reset_pin_set(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct al6021_data_info *data = dev_get_drvdata(dev);
	int ret, value;

	mutex_lock(&(data->mutex_devfile));
	ret = kstrtoint(buf, 0, &value);
	write_reset_pin(data, value);
	mutex_unlock(&(data->mutex_devfile));

	return size;
}

static DEVICE_ATTR(reset_pin, 0644, NULL, al6021_reset_pin_set);

static ssize_t al6021_event_get(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct al6021_data_info *data = dev_get_drvdata(dev);

	if (wait_for_completion_interruptible(&(data->completion_irq_cause)))
		return -ERESTARTSYS;

	return snprintf(buf, 32, "%u\n", data->stats.irq) + 1;
}

static DEVICE_ATTR(event, 0444, al6021_event_get, NULL);

static ssize_t al6021_stats_get(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct al6021_data_info *data = dev_get_drvdata(dev);

	mutex_lock(&(data->mutex_devfile));
	memcpy(buf, &(data->stats), ALTEK_SIZEOF_STATS);
	mutex_unlock(&(data->mutex_devfile));

	return ALTEK_SIZEOF_STATS;
}

static ssize_t al6021_stats_set(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct al6021_data_info *data = dev_get_drvdata(dev);

	mutex_lock(&(data->mutex_devfile));
	memset(&(data->stats), 0, ALTEK_SIZEOF_STATS);
	mutex_unlock(&(data->mutex_devfile));

	return size;
}
static DEVICE_ATTR(stats, 0644, al6021_stats_get, al6021_stats_set);

static ssize_t al6021_cfg_spi_speed_get(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct al6021_data_info *data = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&(data->mutex_devfile));
	ret = snprintf(buf, 32, "%u\n", data->spi->max_speed_hz) + 1;
	mutex_unlock(&(data->mutex_devfile));

	return ret;
}

static ssize_t al6021_cfg_spi_speed_set(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct al6021_data_info *data = dev_get_drvdata(dev);
	int ret, value;

	mutex_lock(&(data->mutex_devfile));
	ret = kstrtoint(buf, 0, &value);
	change_spi_speed(data, value);
	mutex_unlock(&(data->mutex_devfile));

	return size;
}
static DEVICE_ATTR(cfg_spi_speed, 0644, al6021_cfg_spi_speed_get, al6021_cfg_spi_speed_set);

static ssize_t al6021_cfg_writable_get(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct al6021_data_info *data = dev_get_drvdata(dev);

	mutex_lock(&(data->mutex_devfile));

	if (data->read_only)
		*buf++ = '0';
	else
		*buf++ = '1';
	*buf++ = '\n';
	*buf = 0;

	mutex_unlock(&(data->mutex_devfile));

	return 3;
}

static ssize_t al6021_cfg_writable_set(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct al6021_data_info *data = dev_get_drvdata(dev);

	mutex_lock(&(data->mutex_devfile));

	if (!strcasecmp(buf, "0") || !strcasecmp(buf, "n") ||
		!strcasecmp(buf, "no") || !strcasecmp(buf, "off") ||
		!strcasecmp(buf, "disable"))
		data->read_only = 1;
	else
		data->read_only = 0;

	mutex_unlock(&(data->mutex_devfile));

	return size;
}

static DEVICE_ATTR(cfg_writable, 0644, al6021_cfg_writable_get,
			al6021_cfg_writable_set);

#if defined(CONFIG_ALTEK_TEST)
static ssize_t al6021_test_get(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	memcpy(buf, "fake-intr\n", sizeof("fake-intr\n"));
	return strlen(buf);
}

static ssize_t al6021_test_set(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct al6021_data_info *data = dev_get_drvdata(dev);

	//__strip(buf, size);

	if (!strcmp(buf, "fake-intr"))
		al6021_irq_thread(data->irq_num, data);

	return size;
}

static DEVICE_ATTR(test, 0644, al6021_test_get, al6021_test_set);
#endif // defined(CONFIG_ALTEK_TEST)

static struct attribute *al6021_sysfs_attrs[] = {
	&dev_attr_reset_pin.attr,
	&dev_attr_event.attr,
	&dev_attr_stats.attr,
	&dev_attr_cfg_spi_speed.attr,
	&dev_attr_cfg_writable.attr,
#if defined(CONFIG_ALTEK_TEST)
	&dev_attr_test.attr,
#endif
	NULL,
};

static const struct attribute_group al6021_sysfs_attr_grp = {
	.attrs = al6021_sysfs_attrs,
};

int al6021_sysfs_init(struct al6021_data_info *data)
{
	int ret = 0;

	if (!data || !data->devfile)
		return 0;

	ret = sysfs_create_group(&(data->devfile->kobj), &al6021_sysfs_attr_grp);
	return ret;
}

void al6021_sysfs_remove(struct al6021_data_info *data)
{
	if (!data || !data->devfile)
		return;

	sysfs_remove_group(&(data->devfile->kobj), &al6021_sysfs_attr_grp);
	data->devfile = NULL;
}

static int al6021_devfile_open(struct inode *inode, struct file *f)
{
	int ret = 0;

	try_module_get(THIS_MODULE);

	f->private_data = container_of(inode->i_cdev,
				struct al6021_data_info,
				cdev);
	return ret;
}

static int al6021_devfile_release(struct inode *inode, struct file *f)
{
	int ret = 0;

	module_put(THIS_MODULE);
	return ret;
}

#define INSTUCTION_HANDLED 1

static long do_instruction(struct al6021_data_info *data,
				struct al6021_ioparam_s *iop)
{
	iop->result = 0;

	if ((iop->instruction >= INSTRUCTION_RANGE) && al6021_op_is_read(iop->op)) {
		// "read" an instruction is meaningless
		return INSTUCTION_HANDLED;
	}

	if (iop->instruction == 0xffffff00) { // write reset pin
		iop->result = write_reset_pin(data, iop->data_out[0]);
		msleep(1);
		return INSTUCTION_HANDLED;
	}

	if (iop->instruction == 0xfffffff0) { // allow write OPs
		data->read_only = 0;
		return INSTUCTION_HANDLED;
	}

	if (iop->instruction == 0xfffffffc) // reserved
		return INSTUCTION_HANDLED;

	if (iop->instruction == 0xfffffffd) // reserved
		return INSTUCTION_HANDLED;

	if (iop->instruction == 0xfffffffe) { // change SPI speed
		iop->result = change_spi_speed(data, iop->data_out[0]);
		return INSTUCTION_HANDLED;
	}

	if (iop->instruction == 0xffffffff) { // sleep
		msleep(iop->data_out[0]);
		return INSTUCTION_HANDLED;
	}

	return 0; // not handled
}

static int do_spi_operation(struct al6021_data_info *data, struct al6021_ioparam_s *iop,
			u32 size, void **data_in)
{
	int ret = 0;
	u8 bak_op = iop->op;
	u32 bak_addr = iop->addr;
	u8 * p;
	struct spi_transfer	t = {
		.tx_buf = &(iop->op),
		.rx_buf = &(iop->op),
		.len = size,
	};
	struct spi_message m;

	if ( !data->spi)
		return -ENXIO;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	iop->op &= AL6021_OP_SPI_MASK_n;
	p = &(iop->op);
	ret = spi_sync(data->spi, &m);
	if (ret) {
		pr_e("spi_sync failed");
		goto exit;
	}

	iop->op = bak_op;
	iop->addr = bak_addr;

	data->stats.out_bytes += size;
	data->stats.in_bytes += size;

	if (al6021_op_is_read(iop->op)) {
		u32 i = 0;
		u8 *preamble = (u8 *)iop->preamble;

		for (; (i < sizeof(iop->preamble)) && (*preamble != 0xa5);
			i++, preamble++) {
			; // Search for preamble
		}

		if (*preamble != 0xa5) {
			pr_e("Preamble '0xa5' is not found");
			ret = -EIO;
			goto exit;
		}
		*data_in = preamble + 1;
	} else {
		*data_in = iop->data_in;
	}

exit:
	return ret;
}

static int al6021_data_write(struct al6021_data_info *data, unsigned long arg)
{
	int ret = 0;
	struct al6021_ioparam_s iop_basic, *iop_kernel, *iop_user;
	void *data_in;

	mutex_lock(&(data->mutex_devfile));

	for (iop_user = (struct al6021_ioparam_s __user *)arg;
		iop_user != NULL;
		iop_user = (void __user *)iop_basic.next) {

		if (copy_from_user(&iop_basic, iop_user,
				AL6021_SIZEOF_IOPARAM_BASIC)) {
			ret = -EFAULT;
			goto exit;
		}

		data_in = iop_basic.data_out; // temp parking
		iop_basic.result = 0;

		if (do_instruction(data, &iop_basic)) {
			goto final;
		}

		if (iop_basic.op == AL6021_OP_WRITABLE) {
			// allow write OPs
			data->read_only = 0;
			goto final;
		} else if (iop_basic.op == AL6021_OP_GPIO_WRITE) {
			goto final;
		} else if (iop_basic.op == AL6021_OP_GPIO_READ) {
			goto final;
		}

		if (al6021_op_is_write(iop_basic.op) &&
			data->read_only) {
			iop_basic.result = -EROFS;
			goto final;
		}

		if (iop_basic.size > (AL6021_SIZEOF_IOPARAM_BASIC -
			AL6021_SIZEOF_IOPARAM_HEAD)) {
			// it is a long transaction
			if (!data->iop_full ||data->iop_full->size < iop_basic.size) {
				if (data->iop_full)
					devm_kfree(data->dev, data->iop_full);

				data->iop_full =
					devm_kmalloc(data->dev,
						AL6021_SIZEOF_IOPARAM_HEAD +
						iop_basic.size, GFP_KERNEL);
				if (!data->iop_full) {
					pr_e("alloc memory failed");
					ret = -ENOMEM;
					goto exit;
				}

				data->iop_full->size = iop_basic.size;
			}

			if (al6021_op_is_read(iop_basic.op))
				memcpy(&(data->iop_full->op), &(iop_basic.op),
					AL6021_SIZEOF_TXN_HEAD);
			else if (copy_from_user(&(data->iop_full->op),
					&(iop_user->op), iop_basic.size)) {
				ret = -EFAULT;
				goto exit;
			}

			iop_kernel = data->iop_full;
		} else {
			iop_kernel = &iop_basic;
		}

		iop_basic.result = do_spi_operation(data, iop_kernel,
			iop_basic.size, &data_in);

final:
		if ((iop_basic.result == 0) && al6021_op_is_read(iop_basic.op)) {
			if (copy_to_user(&(iop_user->data_in), data_in,
				AL6021_LENOF_DATA_IN(&iop_basic))) {
				ret = -EFAULT;
				goto exit;
			}
		}

		// positive errno for userspace
		iop_basic.result = -(iop_basic.result);

		if (copy_to_user(&(iop_user->result),
			&iop_basic.result,
			sizeof(iop_user->result))) {
			ret = -EFAULT;
			goto exit;
		}

		// negative errno for kernel
		if (iop_basic.result) {
			ret = -(iop_basic.result);
			goto exit;
		}
	}

exit:
	mutex_unlock(&(data->mutex_devfile));

	return ret;
}

static long al6021_devfile_ioctl(struct file *f, unsigned int cmd,
				unsigned long arg)
{
	int ret = 0;
	struct al6021_data_info *data = f->private_data;

	if (_IOC_TYPE(cmd) != AL6021_IOC_MAGIC)
		return -ENODEV;

	switch (cmd) {
	case AL6021_PWR_UP:
		ret = al6021_power_up(data);
		break;
	case AL6021_PWR_DOWN:
		ret = al6021_power_down(data);
		break;
	case AL6021_WRITE_DATA:
	case AL6021_READ_DATA:
		ret = al6021_data_write(data, arg);
		break;
	default:
		break;
	}

	return ret;
}

static const struct file_operations al6021_devfile_ops = {
	.owner = THIS_MODULE,
	.open = al6021_devfile_open,
	.release = al6021_devfile_release,
	.unlocked_ioctl = al6021_devfile_ioctl,
};

int al6021_devfile_init(struct al6021_data_info *data)
{
	int ret = 0;

	if (!data) {
		pr_e("err: invalid parameter");
		ret = EINVAL;
		return ret;
	}

	ret = alloc_chrdev_region(&data->devnum, 0, 1, "altek");
	if (ret < 0) {
		pr_e("alloc chrdev failed, error  = %d", ret);
		return ret;
	}

	cdev_init(&data->cdev, &al6021_devfile_ops);
	data->cdev.owner = THIS_MODULE;

	ret = cdev_add(&data->cdev, data->devnum, 1);
	if (ret < 0) {
		pr_e("err: cdev_add failed");
		goto unregister_chrdev;
	}

	data->driver_class = class_create(THIS_MODULE, "altek");
	if (IS_ERR(data->driver_class)) {
		ret = PTR_ERR(data->driver_class);
		pr_e("err: class_create failed");
		return ret;
	}

	data->devfile = device_create(data->driver_class, NULL, data->devnum,
				data, "al6021");
	if (IS_ERR(data->devfile)) {
		pr_e("err: device_create failed");
		ret = -ENODEV;
		return ret;
	}

	mutex_init(&(data->mutex_devfile));
	ret = al6021_sysfs_init(data);

	return ret;

unregister_chrdev:
	unregister_chrdev_region(data->devnum, 1);
	data->devnum = 0;
	return ret;
}

void al6021_devfile_remove(struct al6021_data_info *data)
{
	if (!data)
		return;

	al6021_sysfs_remove(data);

	device_destroy(data->driver_class, data->devnum);
	class_destroy(data->driver_class);
	data->driver_class = NULL;

	cdev_del(&data->cdev);
	unregister_chrdev_region(data->devnum, 1);
	data->devnum = 0;
}

static void al6021_reset_remove(struct al6021_data_info *data)
{
	if (!data)
		return;

	if (data->reset_gpio)
		devm_gpio_free(data->dev, data->reset_gpio);
	data->reset_gpio = 0;
}

static void al6021_irq_control_process(struct al6021_data_info *data)
{
	struct al6021_ioparam_s iop_basic;
	void *data_in;
	unsigned int txlm_framdone;

#if defined(TIME_DETECTION)
	struct timespec64 time_comm_start, time_comm_end;
	u32 timediff_us=0;
#endif

	data_in = iop_basic.data_out; // temp parking
	iop_basic.result = 0;
	txlm_framdone =  (0x1<<18);
	iop_basic.size = 9;
	//check txlm_b1 frame done status

	mutex_lock(&(data->mutex_devfile));

	iop_basic.op = AL6021_OP_SPI_READ_REG;
	iop_basic.addr = 0xffec2020;
	iop_basic.data_out[0] = 0x0;
	do_spi_operation(data,&iop_basic,iop_basic.size,&data_in);

	if ((*((u32*)data_in) & 0x40000) != 0x40000) {
		mutex_unlock(&(data->mutex_devfile));
		return ;
	} else {
          pr_i("read reg=0x%08x , value=%u (check status) ", iop_basic.addr,*((u32*)data_in));
	}


#if defined(TIME_DETECTION)
	pr_i("irq start -> spi comm");

	getboottime64(&time_comm_start);

	iop_basic.op = AL6021_OP_SPI_READ_REG;
	iop_basic.addr = 0xffec2ac4;
	iop_basic.data_out[0] = 0x0;
	do_spi_operation(data,&iop_basic,iop_basic.size,&data_in);
	pr_i("read reg=0x%08x , value=%u (before spi)", iop_basic.addr,*((u32*)data_in));

	iop_basic.op = AL6021_OP_SPI_READ_REG;
	iop_basic.addr = 0xffec3ac4;
	iop_basic.data_out[0] = 0x0;
	do_spi_operation(data,&iop_basic,iop_basic.size,&data_in);
	pr_i("read reg=0x%08x , value=%u (before spi)", iop_basic.addr,*((u32*)data_in));
#endif

	iop_basic.op = AL6021_OP_SPI_WRITE_REG;
	iop_basic.addr = 0xffec2000;
	iop_basic.data_out[0] = 0x3;
	do_spi_operation(data,&iop_basic,iop_basic.size,&data_in);

	iop_basic.op = AL6021_OP_SPI_WRITE_REG;
	iop_basic.addr = 0xffec2020;
	iop_basic.data_out[0] = txlm_framdone;
	do_spi_operation(data,&iop_basic,iop_basic.size,&data_in);

	iop_basic.op = AL6021_OP_SPI_WRITE_REG;
	iop_basic.addr = 0xffec2000;
	iop_basic.data_out[0] = 0x0;
	do_spi_operation(data,&iop_basic,iop_basic.size,&data_in);

#if defined(TIME_DETECTION)

	iop_basic.op = AL6021_OP_SPI_READ_REG;
	iop_basic.addr = 0xffec2ac4;
	iop_basic.data_out[0] = 0x0;
	do_spi_operation(data,&iop_basic,iop_basic.size,&data_in);
	pr_i("read reg=0x%08x , value=%u (after spi)", iop_basic.addr,*((u32*)data_in));

	iop_basic.op = AL6021_OP_SPI_READ_REG;
	iop_basic.addr = 0xffec3ac4;
	iop_basic.data_out[0] = 0x0;
	do_spi_operation(data,&iop_basic,iop_basic.size,&data_in);
	pr_i("read reg=0x%08x , value=%u (after spi)", iop_basic.addr,*((u32*)data_in));

	getboottime64(&time_comm_end);

	timediff_us = diff_in_us(time_comm_start,time_comm_end);
	if(timediff_us>2500)
		pr_i("time_comm_end-time_comm_start[%u]",timediff_us);
#endif

	mutex_unlock(&(data->mutex_devfile));
}

irqreturn_t al6021_irq_thread(int irq_num, void *_data)
{
	static struct timespec64 time_irq_previous, time_irq_now;
	struct al6021_data_info *data = (struct al6021_data_info *)_data;

#if defined(TIME_DETECTION)
	pr_i("irq start");
	getboottime64(&time_irq_now);
	pr_i("time_irq_now-time_irq_previous[%u]",diff_in_us(time_irq_previous,time_irq_now));
	time_irq_previous = time_irq_now;
#endif

	data->stats.irq++;

	// Evertime an interrupt is occured, there must be something wrong in
	// the device. So we put the driver into READ-ONLY mode, waiting for
	// trouble shooting, until it's informed be writable again, which means
	// the device is going to be reset.
	data->read_only = 0;

	al6021_irq_control_process(data);

	gpio_set_value(data->irq_gpio,0);

	// Trigger thread that is waiting on /sys/class/altek/alxxxx/event
	complete(&(data->completion_irq_cause));

	return IRQ_HANDLED;
}

static void al6021_irq_remove(struct al6021_data_info *data)
{
	if (!data)
		return;

	if (data->irq_num)
		devm_free_irq(data->dev, data->irq_num, data);
	data->irq_num = 0;

	if (data->irq_gpio)
		devm_gpio_free(data->dev, data->irq_gpio);
	data->irq_gpio = 0;
}

static int al6021_regulator_enable(struct al6021_data_info *data,
	enum al6021_rgltr_type type)
{
	int32_t rc = 0;

	if (!data->rgltr[type]) {
		pr_e("Invalid NULL parameter");
		return -EINVAL;
	}

	if (regulator_count_voltages(data->rgltr[type]) > 0) {
		pr_i("rgltr_name %s voltage min=%d, max=%d", data->rgltr_name[type],
			data->rgltr_min_volt[type], data->rgltr_max_volt[type]);

		rc = regulator_set_voltage(data->rgltr[type],
			data->rgltr_min_volt[type], data->rgltr_max_volt[type]);
		if (rc) {
			pr_e("%s set voltage failed", data->rgltr_name[type]);
			return rc;
		}

		rc = regulator_set_load(data->rgltr[type], data->rgltr_op_mode[type]);
		if (rc) {
			pr_e("%s set optimum mode failed", data->rgltr_name[type]);
			return rc;
		}
	}

	rc = regulator_enable(data->rgltr[type]);
	if (rc) {
		pr_e("%s regulator_enable failed", data->rgltr_name[type]);
		return rc;
	}

	return rc;
}

static int al6021_regulator_disable(struct al6021_data_info *data,
	enum al6021_rgltr_type type)
{
	int32_t rc = 0;

	if (!data->rgltr[type]) {
		pr_e("Invalid NULL parameter");
		return -EINVAL;
	}

	rc = regulator_disable(data->rgltr[type]);
	if (rc) {
		pr_e("%s regulator disable failed", data->rgltr_name[type]);
		return rc;
	}

	if (regulator_count_voltages(data->rgltr[type]) > 0) {
		regulator_set_load(data->rgltr[type], 0);
		regulator_set_voltage(data->rgltr[type], 0, data->rgltr_max_volt[type]);
	}

	return rc;
}

static int al6021_clk_enable(struct clk *clk, const char *clk_name,
	int32_t clk_rate)
{
	int rc = 0;
	long clk_rate_round;

	if (!clk || !clk_name)
		return -EINVAL;

	clk_rate_round = clk_round_rate(clk, clk_rate);
	if (clk_rate_round < 0) {
		pr_e("round failed for clock %s rc = %ld",
			clk_name, clk_rate_round);
		return clk_rate_round;
	}
	rc = clk_set_rate(clk, clk_rate_round);
	if (rc) {
		pr_e("set_rate failed on %s", clk_name);
		return rc;
	}
	pr_i("set %s, rate %d, new_rate %ld", clk_name, clk_rate, clk_rate_round);

	rc = clk_prepare_enable(clk);
	if (rc) {
		pr_e("enable failed for %s: rc(%d)", clk_name, rc);
		return rc;
	}

	return rc;
}

static int al6021_clk_disable(struct clk *clk, const char *clk_name)
{
	if (!clk || !clk_name)
		return -EINVAL;

	pr_i("disable %s", clk_name);
	clk_disable_unprepare(clk);

	return 0;
}

static int al6021_power_up(struct al6021_data_info *data)
{
	int rc = 0;

	if (data->power_state == AL6021_POWER_ON) {
		pr_e("al6021 has power on already");
		return rc;
	}

	//vcc 0p9 enable
	rc = al6021_regulator_enable(data, AL6021_RGLTR_VDD);
	usleep_range(1000,1000);

	//vdd 1p8 enable
	rc = al6021_regulator_enable(data, AL6021_RGLTR_VCC);

	//mclk enable
	if (data->pinctrl_info.pinctrl_status) {
		rc = pinctrl_select_state(
			data->pinctrl_info.pinctrl,
			data->pinctrl_info.gpio_state_active);
		if (rc)
			pr_e("cannot set pin to active state");
	}
	rc = al6021_regulator_enable(data, AL6021_RGLTR_VCLK);
	al6021_clk_enable(data->clk, data->clk_name, data->clk_rate);
	usleep_range(4000,4000);

	//reset
	if (data->reset_gpio > 0) {
		gpio_direction_output(data->reset_gpio, 1);
		usleep_range(1000,1000);
		gpio_direction_output(data->reset_gpio, 0);
		usleep_range(1000,1000);
		gpio_direction_output(data->reset_gpio, 1);
		usleep_range(1000,1000);
	}

	data->power_state = AL6021_POWER_ON;
	pr_i("al6021 power on success");

	return rc;
}

static int al6021_power_down(struct al6021_data_info *data)
{
	int rc = 0;

	if (data->power_state == AL6021_POWER_OFF) {
		pr_e("al6021 has power down already");
		return rc;
	}

	//reset
	if (data->reset_gpio > 0)
		gpio_direction_output(data->reset_gpio, 0);
	usleep_range(2000,2000);

	//mclk disable
	al6021_clk_disable(data->clk, data->clk_name);
	al6021_regulator_disable(data, AL6021_RGLTR_VCLK);
	if (data->pinctrl_info.pinctrl_status) {
		rc = pinctrl_select_state(
			data->pinctrl_info.pinctrl,
			data->pinctrl_info.gpio_state_suspend);
		if (rc)
			pr_e("cannot set pin to suspend state");
	}

	//vdd 1p8 disable
	al6021_regulator_disable(data, AL6021_RGLTR_VCC);
	usleep_range(1000,1000);

	//vcc 0p9 disable
	al6021_regulator_disable(data, AL6021_RGLTR_VDD);

	data->power_state = AL6021_POWER_OFF;
	pr_i("al6021 power down success");

	return rc;
}

static int al6021_get_dt_regulator_info
	(struct al6021_data_info *data)
{
	int rc = 0, count = 0, i = 0;
	struct device_node *of_node = data->dev->of_node;

	if (!data || !data->dev) {
		pr_e("Invalid parameters");
		return -EINVAL;
	}

	data->num_rgltr = 0;
	count = of_property_count_strings(of_node, "regulator-names");
	if (count != AL6021_RGLTR_MAX) {
		pr_e("regulators num error");
		return -EINVAL;
	}
	data->num_rgltr = count;

	for (i = 0; i < data->num_rgltr; i++) {
		rc = of_property_read_string_index(of_node,
			"regulator-names", i, &data->rgltr_name[i]);
		pr_i("rgltr_name[%d] = %s", i, data->rgltr_name[i]);
		if (rc) {
			pr_e( "no regulator resource at cnt=%d", i);
			return -ENODEV;
		}
	}

	rc = of_property_read_u32_array(of_node, "rgltr-min-voltage",
		data->rgltr_min_volt, data->num_rgltr);
	if (rc) {
		pr_e("No minimum volatage value found, rc=%d", rc);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(of_node, "rgltr-max-voltage",
		data->rgltr_max_volt, data->num_rgltr);
	if (rc) {
		pr_e("No maximum volatage value found, rc=%d", rc);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(of_node, "rgltr-load-current",
		data->rgltr_op_mode, data->num_rgltr);
	if (rc) {
		pr_e("No Load curent found rc=%d", rc);
		return -EINVAL;
	}

	for (i = 0; i < data->num_rgltr; i++) {
		data->rgltr[i] = regulator_get(data->dev, data->rgltr_name[i]);
		if (IS_ERR_OR_NULL(data->rgltr[i])) {
			pr_e("Regulator %s get failed", data->rgltr_name[i]);
			return -EINVAL;
		}
	}

	return rc;
}

static int al6021_get_dt_clk_info(struct al6021_data_info *data)
{
	int rc = 0;
	int count;
	int num_clk_rates;
	struct device_node *of_node = data->dev->of_node;

	if (!data || !data->dev) {
		pr_e("Invalid parameters");
		return -EINVAL;
	}

	count = of_property_count_strings(of_node, "clock-names");
	if (count != ISPV2_CLK_NUM) {
		pr_e("invalid count of clocks, count=%d", count);
		rc = -EINVAL;
		return rc;
	}

	rc = of_property_read_string(of_node, "clock-names", &data->clk_name);
	if (rc) {
		pr_e("reading clock-names failed");
		return rc;
	}

	num_clk_rates = of_property_count_u32_elems(of_node, "clock-rates");
	if (num_clk_rates <= 0) {
		pr_e("reading clock-rates count failed");
		return -EINVAL;
	}

	rc = of_property_read_u32_index(of_node, "clock-rates",
		0, &data->clk_rate);
	if (rc) {
		pr_e("Error reading clock-rates, rc=%d", rc);
		return rc;
	}

	data->clk_rate = (data->clk_rate == 0) ? (int32_t)NO_SET_RATE : data->clk_rate;
	pr_i("mclk_rate = %d", data->clk_rate);

	data->clk = devm_clk_get(data->dev, data->clk_name);
	if (!data->clk) {
		pr_e("get clk failed for %s", data->clk_name);
		rc = -ENOENT;
		return rc;
	}

	return rc;

}

static int al6021_pinctrl_init(
	struct al6021_pinctrl_info *sensor_pctrl, struct device *dev)
{
	sensor_pctrl->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(sensor_pctrl->pinctrl)) {
		pr_e("Getting pinctrl handle failed");
		return -EINVAL;
	}

	sensor_pctrl->gpio_state_active =
		pinctrl_lookup_state(sensor_pctrl->pinctrl,
			ISPV2_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(sensor_pctrl->gpio_state_active)) {
		pr_e("Failed to get the active state pinctrl handle");
		return -EINVAL;
	}

	sensor_pctrl->gpio_state_suspend = 
		pinctrl_lookup_state(sensor_pctrl->pinctrl,
			ISPV2_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(sensor_pctrl->gpio_state_suspend)) {
		pr_e("Failed to get the suspend state pinctrl handle");
		return -EINVAL;
	}

	return 0;
}

static int al6021_get_dt_data(struct al6021_data_info *data)
{
	int ret = 0;
	struct device_node *np = data->dev->of_node;

	if (!data)
		return 0;

	//get reset pin
	data->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (data->reset_gpio < 0) {
		pr_e("get reset gpio failed");
		return -EINVAL;
	}

	ret = devm_gpio_request(data->dev, data->reset_gpio, "reset-gpio");
	if (ret) {
		pr_e("reset gpio request failed");
		return ret;
	}
	gpio_direction_output(data->reset_gpio, 0);

	//get irq
	data->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (data->irq_gpio < 0) {
		pr_e("get irq gpio failed");
		return -EINVAL;
	}

	ret = devm_gpio_request(data->dev, data->irq_gpio, "irq-gpio");
	if (ret) {
		pr_e("reset irq request failed");
		return ret;
	}

	ret = gpio_direction_input(data->irq_gpio);
	if (ret) {
		pr_e("set irq gpio direction failed");
		return ret;
	}

	//get regulator
	ret = al6021_get_dt_regulator_info(data);
	if (ret) {
		pr_e("get dt regulator failed");
		return ret;
	}

	//get mclk
	ret = al6021_get_dt_clk_info(data);
	if (ret) {
		pr_e("get dt clk failed");
		return ret;
	}

	//get pinctrl
	memset(&(data->pinctrl_info), 0x0, sizeof(data->pinctrl_info));
	ret = al6021_pinctrl_init(&(data->pinctrl_info), data->dev);
	if (ret < 0) {
		data->pinctrl_info.pinctrl_status = 0;
		pr_e("Initialization of pinctrl failed");
		return ret;
	} else {
		data->pinctrl_info.pinctrl_status = 1;
	}

	return ret;
}

////////////////////////////////////////////////////////////////////////////////
static const struct of_device_id al6021_spi_of_match[] = {
	{ .compatible = "altek,al6021_spi", },
	{},
};

static const struct spi_device_id al6021_spi_device_id[] = {
	{ "al6021_spi", 0 },
	{}
};

static int al6021_spi_probe(struct spi_device *spi)
{
	int ret = 0;
	struct device *dev = &(spi->dev);
	struct al6021_data_info *data = NULL;

	spi->max_speed_hz = AL6021_SPI_SPEED_HZ;
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;

	ret = spi_setup(spi);
	if (ret < 0) {
		pr_e("spi_setup failed (%d)", ret);
		return ret;
	}

	data = devm_kzalloc(dev, sizeof(struct al6021_data_info),
			GFP_KERNEL);
	if (!data) {
		pr_e("allco al6021 data failed");
		ret = ENOMEM;
		return ret;
	}

	spi_set_drvdata(spi, data);
	data->spi = spi;
	data->dev = dev;
	data->power_state = AL6021_POWER_OFF;

	ret = al6021_get_dt_data(data);
	if (ret < 0) {
		pr_e("al6021_get_dt_data failed (%d)", ret);
		return ret;
	}

	if (data->irq_gpio > 0) {
		data->irq_num = gpio_to_irq(data->irq_gpio);
		init_completion(&(data->completion_irq_cause));
		ret = devm_request_threaded_irq(data->dev, data->irq_num,
			NULL, al6021_irq_thread,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, NULL, data);
		if (ret) {
			pr_e("request threaded irq failed");
			return ret;
		}
	}

	ret = al6021_devfile_init(data);
	if (ret < 0) {
		pr_e("al6021_devfile_init failed (%d)", ret);
		return ret;
	}
	pr_i("al6021 probe success");

	return ret;
}

static int al6021_spi_remove(struct spi_device *spi)
{
	int ret = 0;
	struct al6021_data_info *data = spi_get_drvdata(spi);
	struct device *dev = data->dev;

	al6021_power_down(data);
	al6021_devfile_remove(data);
	al6021_irq_remove(data);
	al6021_reset_remove(data);
	if (data->iop_full)
		devm_kfree(dev, data->iop_full);
	devm_kfree(dev, data);
	pr_i("al6021 remove success");

	return ret;
}

static struct spi_driver al6021_spi_drv = {
	.driver = {
		.name = "al6021_spi",
		.owner = THIS_MODULE,
		.of_match_table = al6021_spi_of_match,
	},
	.probe = al6021_spi_probe,
	.remove = al6021_spi_remove,
	.id_table = al6021_spi_device_id,
};

static int al6021_init(void)
{
	int ret;

	ret = spi_register_driver(&al6021_spi_drv);
	if (ret < 0) {
		pr_e("spi_register_driver() failed (%d)", ret);
	}

	return ret;
}
static void al6021_exit(void)
{
	spi_unregister_driver(&al6021_spi_drv);
}

module_init(al6021_init);
module_exit(al6021_exit);

////////////////////////////////////////////////////////////////////////////////

MODULE_DESCRIPTION("xiaomi ispv2 driver");
MODULE_LICENSE("GPL");

////////////////////////////////////////////////////////////////////////////////
