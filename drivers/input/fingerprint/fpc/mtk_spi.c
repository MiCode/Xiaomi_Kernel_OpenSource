/*
 * FPC Fingerprint sensor device driver
 *
 * This driver will control the platform resources that the FPC fingerprint
 * sensor needs to operate. The major things are probing the sensor to check
 * that it is actually connected and let the Kernel know this and with that also
 * enabling and disabling of regulators, enabling and disabling of platform
 * clocks.
 * *
 * The driver will expose most of its available functionality in sysfs which
 * enables dynamic control of these features from eg. a user space process.
 *
 * The sensor's IRQ events will be pushed to Kernel's event handling system and
 * are exposed in the drivers event node. This makes it possible for a user
 * space process to poll the input node and receive IRQ events easily. Usually
 * this node is available under /dev/input/eventX where 'X' is a number given by
 * the event system. A user space process will need to traverse all the event
 * nodes and ask for its parent's name (through EVIOCGNAME) which should match
 * the value in device tree named input-device-name.
 *
 *
 * Copyright (c) 2018 Fingerprint Cards AB <tech@fingerprints.com>
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <asm/atomic.h>
#include <linux/timer.h>
#include "cpu_ctrl.h"

#define FPC_DRIVER_FOR_ISEE

#ifdef FPC_DRIVER_FOR_ISEE
#include "teei_fp.h"
#include "tee_client_api.h"
#endif

#ifdef FPC_DRIVER_FOR_ISEE
struct TEEC_UUID uuid_ta_fpc = { 0x7778c03f, 0xc30c, 0x4dd0,
	{0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0x4b}
};
#endif

#define FPC1022_CHIP 0x1000
#define FPC1022_CHIP_MASK_SENSOR_TYPE 0xf000
#define FPC_RESET_LOW_US 5000
#define FPC_RESET_HIGH1_US 100
#define FPC_RESET_HIGH2_US 5000
#define FPC_TTW_HOLD_TIME 1000
#define     FPC102X_REG_HWID      252
u32 spi_speed = 1 * 1000000;

#define PROC_NAME  "hwinfo"
static struct proc_dir_entry *proc_entry;
int mtkfb_prim_panel_unblank(int timeout);
static int cluster_num;
static struct ppm_limit_data *freq_to_set;
static atomic_t boosted = ATOMIC_INIT(0);
static struct timer_list release_timer;
static struct work_struct fp_display_work;
static struct work_struct fp_freq_work;

static const char * const pctl_names[] = {
	"fpsensor_fpc_rst_low",
	"fpsensor_fpc_rst_high",
};

struct fpc_data {
	struct device *dev;
	struct spi_device *spidev;
	struct pinctrl *pinctrl_fpc;
	struct pinctrl_state *pinctrl_state[ARRAY_SIZE(pctl_names)];
	int irq_gpio;
	int rst_gpio;
	bool wakeup_enabled;
	struct wakeup_source ttw_wl;
	bool clocks_enabled;
};
//struct regulator *reg;
static DEFINE_MUTEX(spidev_set_gpio_mutex);
static struct regulator *ldoreg;

extern void mt_spi_disable_master_clk(struct spi_device *spidev);
extern void mt_spi_enable_master_clk(struct spi_device *spidev);
struct spi_device* global_spi =NULL;
bool fpc1022_fp_exist = false;

struct spi_device *spi_fingerprint;
EXPORT_SYMBOL_GPL(global_spi);

static int select_pin_ctl(struct fpc_data *fpc, const char *name)
{
	size_t i;
	int rc;
	struct device *dev = fpc->dev;
	for (i = 0; i < ARRAY_SIZE(fpc->pinctrl_state); i++) {
		const char *n = pctl_names[i];
		if (!strncmp(n, name, strlen(n))) {
			mutex_lock(&spidev_set_gpio_mutex);
			rc = pinctrl_select_state(fpc->pinctrl_fpc, fpc->pinctrl_state[i]);
			mutex_unlock(&spidev_set_gpio_mutex);
			if (rc)
				dev_err(dev, "cannot select '%s'\n", name);
			else
				dev_dbg(dev, "Selected '%s'\n", name);
			goto exit;
		}
	}
	rc = -EINVAL;
	dev_err(dev, "%s:'%s' not found\n", __func__, name);
exit:
	return rc;
}

static int set_clks(struct fpc_data *fpc, bool enable)
{
	int rc = 0;
	if(fpc->clocks_enabled == enable)
		return rc;
	if (enable) {
		mt_spi_enable_master_clk(fpc->spidev);
		fpc->clocks_enabled = true;
		rc = 1;
	} else {
		mt_spi_disable_master_clk(fpc->spidev);
		fpc->clocks_enabled = false;
		rc = 0;
	}

	return rc;
}

static int hw_reset(struct  fpc_data *fpc)
{
	int irq_gpio;
	struct device *dev = fpc->dev;

	select_pin_ctl(fpc, "fpsensor_fpc_rst_high");
	usleep_range(FPC_RESET_HIGH1_US, FPC_RESET_HIGH1_US + 100);

	select_pin_ctl(fpc, "fpsensor_fpc_rst_low");
	usleep_range(FPC_RESET_LOW_US, FPC_RESET_LOW_US + 100);

	select_pin_ctl(fpc, "fpsensor_fpc_rst_high");
	usleep_range(FPC_RESET_HIGH2_US, FPC_RESET_HIGH2_US + 100);

	irq_gpio = gpio_get_value(fpc->irq_gpio);
	dev_info(dev, "IRQ after reset %d\n", irq_gpio);

	dev_info( dev, "Using GPIO# %d as IRQ.\n", fpc->irq_gpio );
	dev_info( dev, "Using GPIO# %d as RST.\n", fpc->rst_gpio );

	return 0;
}

static ssize_t hw_reset_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct  fpc_data *fpc = dev_get_drvdata(dev);

	if (!strncmp(buf, "reset", strlen("reset"))) {
		rc = hw_reset(fpc);
		return rc ? rc : count;
	}
	else
		return -EINVAL;
}
static DEVICE_ATTR(hw_reset, S_IWUSR, NULL, hw_reset_set);

/**
 * sysfs node for controlling whether the driver is allowed
 * to wake up the platform on interrupt.
 */
static ssize_t wakeup_enable_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct  fpc_data *fpc = dev_get_drvdata(dev);
	ssize_t ret = count;

	if (!strncmp(buf, "enable", strlen("enable"))) {
		fpc->wakeup_enabled = true;
		smp_wmb();
	} else if (!strncmp(buf, "disable", strlen("disable"))) {
		fpc->wakeup_enabled = false;
		smp_wmb();
	}
	else
		ret = -EINVAL;

	return ret;
}
static DEVICE_ATTR(wakeup_enable, S_IWUSR, NULL, wakeup_enable_set);

/**
 * sysf node to check the interrupt status of the sensor, the interrupt
 * handler should perform sysf_notify to allow userland to poll the node.
 */
static ssize_t irq_get(struct device *device,
			struct device_attribute *attribute,
			char* buffer)
{
	struct fpc_data *fpc = dev_get_drvdata(device);
	int irq = gpio_get_value(fpc->irq_gpio);

	return scnprintf(buffer, PAGE_SIZE, "%i\n", irq);
}

/**
 * writing to the irq node will just drop a printk message
 * and return success, used for latency measurement.
 */
static ssize_t irq_ack(struct device *device,
			struct device_attribute *attribute,
			const char *buffer, size_t count)
{
	struct fpc_data *fpc = dev_get_drvdata(device);
	dev_dbg(fpc->dev, "%s\n", __func__);

	return count;
}
static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_ack);

static ssize_t clk_enable_set(struct device *device,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc_data *fpc = dev_get_drvdata(device);
	return set_clks(fpc, (*buf == '1')) ? : count;
}
static DEVICE_ATTR(clk_enable, S_IWUSR, NULL, clk_enable_set);

static struct attribute *fpc_attributes[] = {
	&dev_attr_hw_reset.attr,
	&dev_attr_wakeup_enable.attr,
	&dev_attr_clk_enable.attr,
	&dev_attr_irq.attr,
	NULL
};

static const struct attribute_group const fpc_attribute_group = {
	.attrs = fpc_attributes,
};

static ssize_t performance_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count);

static DEVICE_ATTR(performance, S_IRUGO | S_IWUSR, NULL, performance_store);

static struct attribute *performance_attrs[] = {
	&dev_attr_performance.attr,
	NULL
};

static const struct attribute_group performance_attr_group = {
	.attrs = performance_attrs,
	.name = "authen_fd"
};

static void unblank_work(struct work_struct *work)
{
	pr_info(" entry %s line %d \n", __func__, __LINE__);
	mtkfb_prim_panel_unblank(200);
}

static void freq_release(struct work_struct *work)
{
	int i;

	for (i = 0; i < cluster_num; i++) {
		freq_to_set[i].min = -1;
		freq_to_set[i].max = -1;
	}
	if (atomic_read(&boosted) == 1) {
		pr_info("%s  release freq lock\n", __func__);
		update_userlimit_cpu_freq(CPU_KIR_FP, cluster_num, freq_to_set);
		atomic_dec(&boosted);
	}
}

static void freq_release_timer(unsigned long arg)
{
	pr_info(" entry %s line %d \n", __func__, __LINE__);
	schedule_work(&fp_freq_work);
}

static int freq_hold(int sec)
{

	int i;

	for (i = 0; i < cluster_num; i++) {
		freq_to_set[i].min = 2301000;
		freq_to_set[i].max = -1;
	}
	if (atomic_read(&boosted) == 0) {
		pr_info( "%s for %d * 500 msec \n", __func__, sec);
		update_userlimit_cpu_freq(CPU_KIR_FP, cluster_num, freq_to_set);
		atomic_inc(&boosted);
		release_timer.expires = jiffies + (HZ / 2) * sec;
		add_timer(&release_timer);
	}
	schedule_work(&fp_display_work);
	return 0;
}

static ssize_t performance_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	if (!strncmp(buf, "1", count)) {
		pr_info("finger down in authentication/enroll\n");
		freq_hold(1);

	} else if (!strncmp(buf, "0", 1)) {
		pr_info("finger up in authentication/enroll\n");
	} else {
		int timeout;
		if (kstrtoint(buf, 10, &timeout) == 0) {
			freq_hold(timeout);
			pr_info( "hold performance lock for %d * 500ms\n", timeout);
		} else {
			freq_hold(1);
			pr_info("hold performance lock for 500ms\n");
		}
	}
	return count;
}

static irqreturn_t fpc_irq_handler(int irq, void *handle)
{
	struct fpc_data *fpc = handle;
	struct device *dev = fpc->dev;
	static int current_level = 0; // We assume low level from start
	current_level = !current_level;

	if (current_level) {
		dev_dbg(dev, "Reconfigure irq to trigger in low level\n");
		irq_set_irq_type(irq, IRQF_TRIGGER_LOW);
	} else {
		dev_dbg(dev, "Reconfigure irq to trigger in high level\n");
		irq_set_irq_type(irq, IRQF_TRIGGER_HIGH);
	}

	/* Make sure 'wakeup_enabled' is updated before using it
	** since this is interrupt context (other thread...) */
	smp_rmb();
	if (fpc->wakeup_enabled) {
		__pm_wakeup_event(&fpc->ttw_wl, FPC_TTW_HOLD_TIME);
	}

	sysfs_notify(&fpc->dev->kobj, NULL, dev_attr_irq.attr.name);

	return IRQ_HANDLED;
}


static int spi_read_hwid(struct spi_device *spi, u8 * rx_buf)
{
	int error;
	struct spi_message msg;
	struct spi_transfer *xfer;
	u8 tmp_buf[10] = { 0 };
	u8 addr = FPC102X_REG_HWID;

	xfer = kzalloc(sizeof(*xfer) * 2, GFP_KERNEL);
	if (xfer == NULL) {
		dev_err(&spi->dev, "%s, no memory for SPI transfer\n",
			__func__);
		return -ENOMEM;
	}

	spi_message_init(&msg);

	tmp_buf[0] = addr;
	xfer[0].tx_buf = tmp_buf;
	xfer[0].len = 1;
	xfer[0].delay_usecs = 5;

#ifdef CONFIG_SPI_MT65XX
	xfer[0].speed_hz = spi_speed;
	pr_err("%s %d, now spi-clock:%d\n",
	       __func__, __LINE__, xfer[0].speed_hz);
#endif

	spi_message_add_tail(&xfer[0], &msg);

	xfer[1].tx_buf = tmp_buf + 2;
	xfer[1].rx_buf = tmp_buf + 4;
	xfer[1].len = 2;
	xfer[1].delay_usecs = 5;

#ifdef CONFIG_SPI_MT65XX
	xfer[1].speed_hz = spi_speed;
#endif

	spi_message_add_tail(&xfer[1], &msg);
	error = spi_sync(spi, &msg);
	if (error)
		dev_err(&spi->dev, "%s : spi_sync failed.\n", __func__);

	memcpy(rx_buf, (tmp_buf + 4), 2);

	kfree(xfer);
	if (xfer != NULL)
		xfer = NULL;

	return 0;
}

static int check_hwid(struct spi_device *spi)
{
	int error = 0;
	u32 time_out = 0;
	u8 tmp_buf[2] = { 0 };
	u16 hardware_id;

	do {
		spi_read_hwid(spi, tmp_buf);
		pr_info("%s, fpc1520 chip version is 0x%x, 0x%x\n",
		       __func__, tmp_buf[0], tmp_buf[1]);

		time_out++;

		hardware_id = ((tmp_buf[0] << 8) | (tmp_buf[1]));
		pr_err("fpc hardware_id[0]= 0x%x id[1]=0x%x\n", tmp_buf[0],
		       tmp_buf[1]);

		if ((FPC1022_CHIP_MASK_SENSOR_TYPE & hardware_id) ==
		    FPC1022_CHIP) {
			pr_err("fpc match hardware_id = 0x%x is true\n",
			       hardware_id);
			error = 0;
		} else {
			pr_err("fpc match hardware_id = 0x%x is failed\n",
			       hardware_id);
			error = -1;
		}

		if (!error) {
			pr_info("fpc %s, fpc1022 chip version check pass, time_out=%d\n",
			       __func__, time_out);
			return 0;
		}
	} while (time_out < 2);

	pr_info("%s, fpc1022 chip version read failed, time_out=%d\n",
	       __func__, time_out);
	spi_fingerprint = spi;
	return -1;
}

static int proc_show_ver(struct seq_file *file,void *v)
{
	seq_printf(file,"Fingerprint: FPC\n");
	return 0;
}

static int proc_open(struct inode *inode,struct file *file)
{
	pr_info("fpc proc_open\n");
	single_open(file,proc_show_ver,NULL);
	return 0;
}

static const struct file_operations proc_file_fpc_ops = {
	.owner = THIS_MODULE,
	.open = proc_open,
	.read = seq_read,
	.release = single_release,
};


static int fpc_power_supply(struct fpc_data *fpc)
{
	int ret = 0;
	struct device *dev = &fpc->spidev->dev;
	pr_info("fp Power init start");
	// dev:i2c client device or spi slave device

	ldoreg = regulator_get(dev, "VFP");
	if (IS_ERR(ldoreg)) {
		pr_err("regulator_get()=%d failed!\n",ldoreg);
		ldoreg = NULL;
		return -1;
	}

	ret = regulator_set_voltage(ldoreg, 3300000, 3300000);
	if (ret)
		pr_err("regulator_set_voltage(%d) failed!\n", ret);

	regulator_enable(ldoreg);
	if (ret)
		pr_err("regulator_enable(%d) failed!\n", ret);
	
	pr_info("fp Power init OK");
	return ret;
}


static int mtk6765_probe(struct spi_device *spidev)
{
	struct device *dev = &spidev->dev;
	struct device_node *node_eint;
	struct fpc_data *fpc;
	int irqf = 0;
	int irq_num = 0;
	int rc = 0;
	int fpc_sensor_exit  = 0;
	size_t i;
	global_spi = spidev;

	dev_dbg(dev, "%s\n", __func__);

	spidev->dev.of_node = of_find_compatible_node(NULL, NULL, "mediatek,fingerprint");
	if (!spidev->dev.of_node) {
		dev_err(dev, "no of node found\n");
		rc = -EINVAL;
		goto exit;
	}

	fpc = devm_kzalloc(dev, sizeof(*fpc), GFP_KERNEL);
	if (!fpc) {
		dev_err(dev, "failed to allocate memory for struct fpc_data\n");
		rc = -ENOMEM;
		goto exit;
	}

	cluster_num = arch_get_nr_clusters();
	dev_err(dev, "cluster_num = %d \n", cluster_num);

	freq_to_set =
	    kcalloc(cluster_num, sizeof(struct ppm_limit_data), GFP_KERNEL);

	if (!freq_to_set) {
		dev_err(dev, "kcalloc freq_to_set fail\n");
		goto exit;
	}

	INIT_WORK(&fp_freq_work, freq_release);
	INIT_WORK(&fp_display_work, unblank_work);

	init_timer(&release_timer);
	release_timer.function = freq_release_timer;
	release_timer.data = 0UL;

	fpc->dev = dev;
	dev_set_drvdata(dev, fpc);
	fpc->spidev = spidev;
	fpc->spidev->irq = 0; /*SPI_MODE_0*/
	fpc->spidev->mode = SPI_MODE_0;
	fpc->spidev->bits_per_word = 8;
	fpc->spidev->max_speed_hz = 1 * 1000 * 1000;
	
	fpc->pinctrl_fpc = devm_pinctrl_get(&spidev->dev);
	if (IS_ERR(fpc->pinctrl_fpc)) {
		rc = PTR_ERR(fpc->pinctrl_fpc);
		dev_err(fpc->dev, "Cannot find pinctrl_fpc rc = %d.\n", rc);
		goto exit;
	}

	for (i = 0; i < ARRAY_SIZE(fpc->pinctrl_state); i++) {
		const char *n = pctl_names[i];
		struct pinctrl_state *state = pinctrl_lookup_state(fpc->pinctrl_fpc, n);
		if (IS_ERR(state)) {
			dev_err(dev, "cannot find '%s'\n", n);
			rc = -EINVAL;
			goto exit;
		}
		dev_info(dev, "found pin control %s\n", n);
		fpc->pinctrl_state[i] = state;
	}

	rc = sysfs_create_group(&dev->kobj, &performance_attr_group);
	if (rc) {
		dev_err(dev, "could not create performance_attr_group sysfs\n");
		goto exit;
	}

	fpc_power_supply(fpc);
	fpc->clocks_enabled = false;
	set_clks(fpc, true);
	(void)hw_reset(fpc);
	
	fpc_sensor_exit = check_hwid(spidev);
	if (fpc_sensor_exit < 0) {
			pr_notice("%s: %d get chipid fail. now exit\n",
				  __func__, __LINE__);
			devm_pinctrl_put(fpc->pinctrl_fpc);
			gpio_free(fpc->rst_gpio);
			set_clks(fpc,false );
			fpc1022_fp_exist = false;
			return 0;
	}
	fpc1022_fp_exist = true;
	#ifdef FPC_DRIVER_FOR_ISEE
	memcpy(&uuid_fp, &uuid_ta_fpc, sizeof(struct TEEC_UUID));
	#endif

	node_eint = of_find_compatible_node(NULL, NULL, "mediatek,fpsensor_fp_eint");
	if (node_eint == NULL) {
		rc = -EINVAL;
		dev_err(fpc->dev, "cannot find node_eint rc = %d.\n", rc);
		goto exit;
	}

	fpc->irq_gpio = of_get_named_gpio(node_eint, "int-gpios", 0);
	irq_num = irq_of_parse_and_map(node_eint, 0);/*get irq num*/
	if (!irq_num) {
		rc = -EINVAL;
		dev_err(fpc->dev, "get irq_num error rc = %d.\n", rc);
		goto exit;
	}

	dev_dbg(dev, "Using GPIO#%d as IRQ.\n", fpc->irq_gpio);
	dev_dbg(dev, "Using GPIO#%d as RST.\n", fpc->rst_gpio);

	fpc->wakeup_enabled = false;

	irqf = IRQF_TRIGGER_HIGH | IRQF_ONESHOT;
	if (of_property_read_bool(dev->of_node, "fpc,enable-wakeup")) {
		irqf |= IRQF_NO_SUSPEND;
		device_init_wakeup(dev, 1);
	}
	rc = devm_request_threaded_irq(dev, irq_num,
		NULL, fpc_irq_handler, irqf,
		dev_name(dev), fpc);
	if (rc) {
		dev_err(dev, "could not request irq %d\n", irq_num);
		goto exit;
	}
	dev_dbg(dev, "requested irq %d\n", irq_num);

	/* Request that the interrupt should be wakeable */
	enable_irq_wake(irq_num);
	wakeup_source_init(&fpc->ttw_wl, "fpc_ttw_wl");

	rc = sysfs_create_group(&dev->kobj, &fpc_attribute_group);
	if (rc) {
		dev_err(dev, "could not create sysfs\n");
		goto exit;
	}

	(void)hw_reset(fpc);
	
	proc_entry = proc_create(PROC_NAME, 0644, NULL, &proc_file_fpc_ops);
	if (NULL == proc_entry) {
		pr_err("fpc1020 Couldn't create proc entry!");
		return -ENOMEM;
	} else {
		pr_err("fpc1020 Create proc entry success!");
	}

	dev_info(dev, "%s: ok\n", __func__);
exit:
	set_clks(fpc,false );
	return rc;
}

static int mtk6765_remove(struct spi_device *spidev)
{
	struct  fpc_data *fpc = dev_get_drvdata(&spidev->dev);

	sysfs_remove_group(&spidev->dev.kobj, &fpc_attribute_group);
	wakeup_source_trash(&fpc->ttw_wl);
	remove_proc_entry(PROC_NAME,NULL);
	dev_info(&spidev->dev, "%s\n", __func__);

	return 0;
}

static struct of_device_id mt6765_of_match[] = {
	{ .compatible = "fpc,fpc_spi", },
	{}
};
MODULE_DEVICE_TABLE(of, mt6765_of_match);

static struct spi_driver mtk6765_driver = {
	.driver = {
		.name	= "fpc_spi",
		.bus = &spi_bus_type,
		.owner	= THIS_MODULE,
		.of_match_table = mt6765_of_match,
	},
	.probe	= mtk6765_probe,
	.remove	= mtk6765_remove
};

static int __init fpc_sensor_init(void)
{
	int status;

	status = spi_register_driver(&mtk6765_driver);
	if (status < 0) {
		pr_info("%s, fpc_sensor_init failed.\n", __func__);
	}

	return status;
}
//module_init(fpc_sensor_init);
late_initcall(fpc_sensor_init);
static void __exit fpc_sensor_exit(void)
{
	spi_unregister_driver(&mtk6765_driver);
}
module_exit(fpc_sensor_exit);

MODULE_LICENSE("GPL");
