/* Copyright (C) 2013 by Xiang Xiao <xiaoxiang@xiaomi.com>
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <media/pwm-ir.h>
#include <media/rc-core.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#define IR_VDD_MIN_UV 2600000
#define IR_VDD_MAX_UV 3300000
#define IR_DEBUG_FS 1
#if IR_DEBUG_FS
#include <linux/slab.h>
#include <linux/proc_fs.h>
static struct proc_dir_entry *g_lct_ir_debug_proc = NULL;
#define LCT_IR_DEBUG_PROC_FILE "pwm_ir_fs"
#endif
struct pwm_ir_dev {
	struct mutex            lock;
	struct platform_device *pdev;
	struct rc_dev          *rdev;
	struct regulator       *reg;
	struct pwm_device      *pwm;
	int 					pwm_enable_gpio;
	u32                     carrier;
	u32                     duty_cycle;
};
struct pwm_ir_packet {
	struct completion  done;
	struct hrtimer     timer;
	struct pwm_device *pwm;
	bool               abort;
	unsigned int      *buffer;
	unsigned int       length;
	unsigned int       next;
	struct mutex       lock;
};
#if IR_DEBUG_FS
struct pwm_ir_dev *g_ir_dev = NULL;
#endif
#define __devexit
#define __devinitdata
#define __devinit
#define __devexit_p
static int pwm_ir_tx_config(struct pwm_ir_dev *dev, u32 carrier, u32 duty_cycle)
{
	int period_ns, duty_ns, rc;
	rc = stop_ir_pwm_data();
	period_ns = NSEC_PER_SEC / carrier;
	duty_ns = period_ns * duty_cycle / 100;
	printk("pwm_ir_tx_config period_ns = %d,duty_ns = %d \n",period_ns,duty_ns);
	rc = pwm_config(dev->pwm, duty_ns, period_ns);
	if (rc == 0) {
		dev->carrier = carrier;
		dev->duty_cycle = duty_cycle;
	}
	return rc;
}
static int pwm_ir_tx_carrier(struct rc_dev *rdev, u32 carrier)
{
	struct pwm_ir_dev *dev = rdev->priv;
	int rc;
	mutex_lock(&dev->lock);
	rc = pwm_ir_tx_config(dev, carrier, dev->duty_cycle);
	mutex_unlock(&dev->lock);
	return rc;
}
static int pwm_ir_tx_duty_cycle(struct rc_dev *rdev, u32 duty_cycle)
{
	struct pwm_ir_dev *dev = rdev->priv;
	int rc;
	mutex_lock(&dev->lock);
	rc = pwm_ir_tx_config(dev, dev->carrier, duty_cycle);
	mutex_unlock(&dev->lock);
	return rc;
}
static enum hrtimer_restart pwm_ir_tx_timer(struct hrtimer *timer)
{
	struct pwm_ir_packet *pkt = container_of(timer, struct pwm_ir_packet, timer);
	enum hrtimer_restart restart = HRTIMER_RESTART;
	if (!pkt->abort && pkt->next < pkt->length) {
		u64 orun = hrtimer_forward_now(&pkt->timer,
			ns_to_ktime(pkt->buffer[pkt->next++]));
		if (orun > 1)
			pr_warn("pwm-ir: lost %llu hrtimer callback\n", orun - 1);
		if (pkt->next & 0x01)
			pwm_disable(pkt->pwm);
		else /* pulse */
			pwm_enable(pkt->pwm);
	} else {
		restart = HRTIMER_NORESTART;
		pwm_disable(pkt->pwm);
		complete(&pkt->done);
	}
	return restart;
}
static int pwm_ir_tx_transmit_with_timer(struct pwm_ir_packet *pkt)
{
	int rc = 0;
	init_completion(&pkt->done);
	hrtimer_init(&pkt->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pkt->timer.function = pwm_ir_tx_timer;
	hrtimer_start(&pkt->timer, ns_to_ktime(0), HRTIMER_MODE_REL);
	rc = wait_for_completion_interruptible(&pkt->done);
	if (rc != 0) { /* signal exit immediately */
		pkt->abort = true;
		wait_for_completion(&pkt->done);
	}
	return pkt->next ? : -ERESTARTSYS;
}
static long pwm_ir_tx_work(void *arg)
{
	unsigned long flags;
	long rc=0;
	local_irq_save(flags);
	rc = qpnp_ir_pwm_data(arg);
	local_irq_restore(flags);
	return rc;
}
static int pwm_ir_tx_transmit_with_delay(struct pwm_ir_packet *pkt)
{
	int cpu, rc = -ENODEV;
	for_each_online_cpu(cpu) {
		if (cpu != 0) {
			rc = work_on_cpu(cpu, pwm_ir_tx_work, pkt);
			break;
		}
	}
	if (rc == -ENODEV) {
		pr_warn("pwm-ir: can't run on the auxilliary cpu\n");
		rc = pwm_ir_tx_work(pkt);
	}
	return rc;
}
static int pwm_ir_tx_transmit(struct rc_dev *rdev, unsigned *txbuf, unsigned n)
{
	struct pwm_ir_dev *dev = rdev->priv;
	struct pwm_ir_data *data = dev->pdev->dev.platform_data;
	struct pwm_ir_packet pkt = {};
	int i = 0, rc = 0;
	unsigned int temp = 0, temp2 = 0;
	for (i = 0; i < n; i++) {
		txbuf[i] *= NSEC_PER_USEC;
		temp  = txbuf[i] / 26666;
		temp2 = txbuf[i] % 26666;
		if(((i+1) % 2) == 0) {
			if((txbuf[i] > 53332) && (txbuf[i] < 280000))
				txbuf[i] = temp * 26666  - 16000;
			else if((txbuf[i] > 280000) && (txbuf[i] < 620000))
			{
				if(txbuf[i] > 53332) {
					if(temp2 < 8000)
						txbuf[i] = temp * 26666;
					else
						txbuf[i] = temp * 26666 + 16000;
				}
			}
		} else{
			if(txbuf[i] > 53332) {
				if(temp2 < 8000)
					txbuf[i] = temp * 26666;
				else
					txbuf[i] = (temp + 1) * 26666;
			}
		}
	}
	mutex_lock(&dev->lock);
	if (dev->reg) {
		rc = regulator_enable(dev->reg);
		if (rc != 0)
			goto err_regulator_enable;
	}
	pkt.pwm    = dev->pwm;
	pkt.buffer = txbuf;
	pkt.length = n;
	if (data->use_timer)
		rc = pwm_ir_tx_transmit_with_timer(&pkt);
	else
		rc = pwm_ir_tx_transmit_with_delay(&pkt);
	if (dev->reg)
		regulator_disable(dev->reg);
err_regulator_enable:
	mutex_unlock(&dev->lock);
	return rc;
}
static int __devinit pwm_ir_tx_probe(struct pwm_ir_dev *dev)
{
	struct pwm_ir_data *data = dev->pdev->dev.platform_data;
	struct platform_device *pdev = dev->pdev;
	int rc = 0;
	if (data->reg_id) {
		dev->reg = regulator_get(&dev->pdev->dev, data->reg_id);
		if (IS_ERR(dev->reg)) {
			dev_err(&dev->pdev->dev,
				"failed to regulator_get(%s)\n",
				 data->reg_id);
			return PTR_ERR(dev->reg);
		}
	}
	if (regulator_count_voltages(dev->reg) > 0) {
		rc = regulator_set_voltage(dev->reg, IR_VDD_MIN_UV,
					IR_VDD_MAX_UV);
		if (rc) {
			dev_err(&dev->pdev->dev,
				"Regulator set_vtg failed vdd rc=%d\n",
					rc);
			goto err_regulator_put;
		}
	}
	dev->pwm = of_pwm_get(pdev->dev.of_node, NULL);
	if (IS_ERR(dev->pwm)) {
		dev_err(&dev->pdev->dev,
			"failed to of_pwm_get()\n");
		rc = PTR_ERR(dev->pwm);
		dev_err(&dev->pdev->dev, "Cannot get PWM device rc:(%d)\n", rc);
		dev->pwm = NULL;
		goto err_regulator_put;
	}
	if (data->low_active) {
#if 0 /* need the latest kernel */
		rc = pwm_set_polarity(dev->pwm, PWM_POLARITY_INVERSED);
#else
		rc = -ENOSYS;
#endif
		if (rc != 0) {
			dev_err(&dev->pdev->dev, "failed to change polarity\n");
			goto err_pwm_free;
		}
	}
	rc = pwm_ir_tx_config(dev, 38000, 33);
	if (rc != 0) {
		dev_err(&dev->pdev->dev, "failed to change carrier and duty\n");
		goto err_pwm_free;
	}
	dev->rdev->tx_ir           = pwm_ir_tx_transmit;
	dev->rdev->s_tx_carrier    = pwm_ir_tx_carrier;
	dev->rdev->s_tx_duty_cycle = pwm_ir_tx_duty_cycle;
	return rc;
err_pwm_free:
	pwm_free(dev->pwm);
err_regulator_put:
	if (dev->reg)
		regulator_put(dev->reg);
	return rc;
}
static void pwm_ir_tx_remove(struct pwm_ir_dev *dev)
{
	if (dev->reg)
		regulator_put(dev->reg);
	pwm_free(dev->pwm);
}
#if IR_DEBUG_FS
static ssize_t lct_ir_debug_proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	char data[256] = {0};
	char tmp_data[256] = {0};
	char *operation = NULL;
	char* value = NULL;
	char *tmpValue1 = NULL;
	char *tmpValue2 = NULL;
	int time[256] = {0};
	int time_count = 0;
	int frequency_HZ = 0;
	int duty_percent = 0;
	int rc;
	if(copy_from_user(tmp_data, buf, size)) {
		 printk("copy_from_user() fail.\n");
		 return -EFAULT;
	}
	if(g_ir_dev == NULL) {
		printk("lct_ir_debug_proc_write Please check probe func \n");
		return -EFAULT;
	}
	 printk("lct_ir_debug_proc_write tmp_data = %s \n",tmp_data);
	 strcpy(data,tmp_data);
	 value = strchr(tmp_data,':');
	 if(value != NULL) {
		data[value-tmp_data] = '\0';
		operation = data;
		value++;
	 }
	 printk("lct_ir_debug_proc_write operation = %s \n",operation);
	 if(strcmp(operation,"sf") == 0) {
		frequency_HZ = simple_strtoul(value, NULL, 10);
		printk("lct_ir_debug_proc_write frequency_HZ=%d \n",frequency_HZ);
		rc = pwm_ir_tx_carrier(g_ir_dev->rdev,(u32)frequency_HZ);
		printk("lct_ir_debug_proc_write set frequency_HZ rc=%d \n",rc);
	 } else if(strcmp(operation,"sd") == 0) {
		duty_percent = simple_strtoul(value, NULL, 10);
		printk("lct_ir_debug_proc_write duty_percent=%d \n",duty_percent);
		rc = pwm_ir_tx_duty_cycle(g_ir_dev->rdev,(u32)duty_percent);
		printk("lct_ir_debug_proc_write set duty_percent rc=%d \n",rc);
	 } else if(strcmp(operation,"dispwm") == 0) {
		printk("lct_ir_debug_proc_write stop PWM output \n");
		pwm_disable(g_ir_dev->pwm);
	 }
	else if(strcmp(operation,"enpwm") == 0) {
		printk("lct_ir_debug_proc_write stop PWM output \n");
		rc = pwm_enable(g_ir_dev->pwm);
		printk("lct_ir_debug_proc_write pwm_enable result = %d \n",rc);
	} else if(strcmp(operation,"tx") == 0) {
		printk("lct_ir_debug_proc_write time value = %s \n",operation);
		tmpValue2 = value;
		while(((tmpValue1 = strchr(tmpValue2,':')) != NULL) || (time_count >= 50)) {
			printk("lct_ir_debug_proc_write time tmpValue1 = %s \n",tmpValue1);
			value[tmpValue1 - tmpValue2] = '\0';
			time[time_count++] = simple_strtoul(value, NULL, 10);
			tmpValue1++;
			tmpValue2 = tmpValue1;
			value = tmpValue2;;
		}
		if(time_count < 50 ) {
			time[time_count++] = simple_strtoul(value, NULL, 10);
		}
		rc = pwm_ir_tx_transmit(g_ir_dev->rdev,(unsigned*)time,(unsigned)time_count);
		printk("lct_ir_debug_proc_write tx result = %d \n",rc);
	} else {
		printk("lct_ir_debug_proc_write invalid operation = %s \n",operation);
		return -EFAULT;
	}
	return size;
}
static ssize_t lct_ir_debug_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int cnt= 0;
	char *page = NULL;
	page = kzalloc(128, GFP_KERNEL);
	cnt = sprintf(page, "%s", "usage: echo \"1:38000:50\" > proc/pwm_ir_fs\n");
	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	kfree(page);
	return cnt;
}
static const struct file_operations lct_ir_debug_proc_fops = {
	.read		= lct_ir_debug_proc_read,
	.write		= lct_ir_debug_proc_write,
};
#endif
static int __devinit pwm_ir_probe(struct platform_device *pdev)
{
	struct pwm_ir_dev *dev;
	int rc = -ENOMEM;
	if (!pdev->dev.platform_data) {
		pdev->dev.platform_data = devm_kzalloc(&pdev->dev, sizeof(struct pwm_ir_data), GFP_KERNEL);
		if (pdev->dev.platform_data) {
			if (pdev->dev.of_node) {
				struct pwm_ir_data *data = pdev->dev.platform_data;
				of_property_read_string(pdev->dev.of_node, "reg-id", &data->reg_id);
				data->low_active = of_property_read_bool(pdev->dev.of_node, "low-active");
				data->use_timer = of_property_read_bool(pdev->dev.of_node, "use-timer");
	       	dev_info(&pdev->dev,
					 "reg-id = %s, low-active = %d, use-timer = %d\n",
					  data->reg_id,  data->low_active, data->use_timer);
			}
		} else {
			dev_err(&pdev->dev, "failed to alloc platform data\n");
			return -ENOMEM;
		}
	}
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev, "failed to alloc dev\n");
		return rc;
	}
	mutex_init(&dev->lock);
	dev->pdev = pdev;
	platform_set_drvdata(pdev, dev);
	dev->rdev = rc_allocate_device();
	if (!dev->rdev) {
		dev_err(&pdev->dev, "failed to alloc rdev\n");
		goto err_rc_allocate_device;
	}
	dev->rdev->dev.parent       = &pdev->dev;
	dev->rdev->input_name       = PWM_IR_NAME;
	dev->rdev->input_phys       = PWM_IR_NAME;
	dev->rdev->input_id.bustype = BUS_HOST;
	dev->rdev->driver_name      = PWM_IR_NAME;
	dev->rdev->map_name         = RC_MAP_LIRC;
	dev->rdev->driver_type      = RC_DRIVER_IR_RAW;
	dev->rdev->priv             = dev;
	rc = pwm_ir_tx_probe(dev);
	if (rc != 0)
		goto err_pwm_ir_tx_probe;
	rc = rc_register_device(dev->rdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "failed to register rdev\n");
		goto err_rc_register_device;
	}
#if IR_DEBUG_FS
	g_lct_ir_debug_proc = proc_create_data(LCT_IR_DEBUG_PROC_FILE, 0660, NULL, &lct_ir_debug_proc_fops, NULL);
	if (IS_ERR_OR_NULL(g_lct_ir_debug_proc)) {
		printk("pwm_ir_probe create_proc_entry g_lct_ir_debug_proc failed\n");
	} else {
		printk("pwm_ir_probe create_proc_entry g_lct_ir_debug_proc success\n");
	}
	g_ir_dev = dev;
#endif
	return rc;
err_rc_register_device:
	pwm_ir_tx_remove(dev);
err_pwm_ir_tx_probe:
	rc_free_device(dev->rdev);
err_rc_allocate_device:
	kfree(dev);
	return rc;
}
static int __devexit pwm_ir_remove(struct platform_device *pdev)
{
	struct pwm_ir_dev *dev = platform_get_drvdata(pdev);
	rc_unregister_device(dev->rdev);
	pwm_ir_tx_remove(dev);
	kfree(dev);
	return 0;
}
/* code for init and exit */
static const struct of_device_id of_pwm_ir_match[] = {
	{.compatible = PWM_IR_NAME,},
	{},
};
static struct platform_driver pwm_ir_driver = {
	.probe  = pwm_ir_probe,
	.remove = __devexit_p(pwm_ir_remove),
	.driver = {
		.name  = PWM_IR_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_pwm_ir_match,
	},
};
static int __init pwm_ir_init(void)
{
	return platform_driver_register(&pwm_ir_driver);
}
late_initcall(pwm_ir_init);
static void __exit pwm_ir_exit(void)
{
	platform_driver_unregister(&pwm_ir_driver);
}
module_exit(pwm_ir_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("lct <lct@loncheer.com>");
MODULE_DESCRIPTION("PWM IR driver");
