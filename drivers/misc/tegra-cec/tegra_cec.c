/*
 * drivers/misc/tegra-cec/tegra_cec.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk/tegra.h>

#include "tegra_cec.h"

static ssize_t cec_logical_addr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t cec_logical_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static DEVICE_ATTR(cec_logical_addr_config, S_IWUSR | S_IRUGO,
		cec_logical_addr_show, cec_logical_addr_store);

int tegra_cec_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct tegra_cec *cec = container_of(miscdev,
		struct tegra_cec, misc_dev);
	dev_dbg(cec->dev, "%s\n", __func__);

	wait_event_interruptible(cec->init_waitq,
	    atomic_read(&cec->init_done) == 1);
	file->private_data = cec;

	return 0;
}

int tegra_cec_release(struct inode *inode, struct file *file)
{
	struct tegra_cec *cec = file->private_data;

	dev_dbg(cec->dev, "%s\n", __func__);

	return 0;
}

ssize_t tegra_cec_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	struct tegra_cec *cec = file->private_data;
	unsigned long write_buff;

	count = 4;

	wait_event_interruptible(cec->init_waitq,
	    atomic_read(&cec->init_done) == 1);

	if (copy_from_user(&write_buff, buffer, count))
		return -EFAULT;

	writel((TEGRA_CEC_INT_MASK_TX_REGISTER_EMPTY |
		TEGRA_CEC_INT_MASK_TX_REGISTER_UNDERRUN |
	    TEGRA_CEC_INT_MASK_TX_FRAME_OR_BLOCK_NAKD |
	    TEGRA_CEC_INT_MASK_TX_ARBITRATION_FAILED |
	    TEGRA_CEC_INT_MASK_TX_BUS_ANOMALY_DETECTED |
	    TEGRA_CEC_INT_MASK_RX_REGISTER_FULL |
	    TEGRA_CEC_INT_MASK_RX_REGISTER_OVERRUN),
	    cec->cec_base + TEGRA_CEC_INT_MASK);

	wait_event_interruptible(cec->tx_waitq, cec->tx_wake == 1);
	writel(write_buff, cec->cec_base + TEGRA_CEC_TX_REGISTER);
	cec->tx_wake = 0;

	writel((TEGRA_CEC_INT_MASK_TX_REGISTER_UNDERRUN |
		TEGRA_CEC_INT_MASK_TX_FRAME_OR_BLOCK_NAKD |
	    TEGRA_CEC_INT_MASK_TX_ARBITRATION_FAILED |
	    TEGRA_CEC_INT_MASK_TX_BUS_ANOMALY_DETECTED |
	    TEGRA_CEC_INT_MASK_RX_REGISTER_FULL |
	    TEGRA_CEC_INT_MASK_RX_REGISTER_OVERRUN),
	    cec->cec_base + TEGRA_CEC_INT_MASK);

	write_buff = 0x00;
	return count;
}

ssize_t tegra_cec_read(struct file *file, char  __user *buffer,
	size_t count, loff_t *ppos)
{
	struct tegra_cec *cec = file->private_data;
	count = 2;

	wait_event_interruptible(cec->init_waitq,
	    atomic_read(&cec->init_done) == 1);

	if (cec->rx_wake == 0)
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

	wait_event_interruptible(cec->rx_waitq, cec->rx_wake == 1);

	if (copy_to_user(buffer, &(cec->rx_buffer), count))
		return -EFAULT;

	cec->rx_buffer = 0x0;
	cec->rx_wake = 0;
	return count;
}

static irqreturn_t tegra_cec_irq_handler(int irq, void *data)
{
	struct device *dev = data;
	struct tegra_cec *cec = dev_get_drvdata(dev);
	unsigned long status;

	status = readl(cec->cec_base + TEGRA_CEC_INT_STAT);

	if (!status)
		return IRQ_HANDLED;

	if ((status & TEGRA_CEC_INT_STAT_RX_REGISTER_OVERRUN) ||
		(status & TEGRA_CEC_INT_STAT_RX_BUS_ANOMALY_DETECTED) ||
		(status & TEGRA_CEC_INT_STAT_RX_START_BIT_DETECTED) ||
		(status & TEGRA_CEC_INT_STAT_RX_BUS_ERROR_DETECTED)) {
		writel((TEGRA_CEC_INT_STAT_RX_REGISTER_OVERRUN |
			TEGRA_CEC_INT_STAT_RX_BUS_ANOMALY_DETECTED |
			TEGRA_CEC_INT_STAT_RX_START_BIT_DETECTED |
			TEGRA_CEC_INT_STAT_RX_BUS_ERROR_DETECTED),
			cec->cec_base + TEGRA_CEC_INT_STAT);
	} else if (status & TEGRA_CEC_INT_STAT_RX_REGISTER_FULL) {
		writel((TEGRA_CEC_INT_STAT_RX_REGISTER_FULL),
			cec->cec_base + TEGRA_CEC_INT_STAT);
		cec->rx_buffer = readw(cec->cec_base + TEGRA_CEC_RX_REGISTER);
		cec->rx_wake = 1;
		wake_up_interruptible(&cec->rx_waitq);
	} else if ((status & TEGRA_CEC_INT_STAT_TX_REGISTER_UNDERRUN) ||
		(status & TEGRA_CEC_INT_STAT_TX_FRAME_OR_BLOCK_NAKD) ||
		(status & TEGRA_CEC_INT_STAT_TX_ARBITRATION_FAILED) ||
		(status & TEGRA_CEC_INT_STAT_TX_BUS_ANOMALY_DETECTED)) {
		writel((TEGRA_CEC_INT_STAT_TX_REGISTER_UNDERRUN |
			TEGRA_CEC_INT_STAT_TX_FRAME_OR_BLOCK_NAKD |
			TEGRA_CEC_INT_STAT_TX_REGISTER_EMPTY |
			TEGRA_CEC_INT_STAT_TX_ARBITRATION_FAILED |
			TEGRA_CEC_INT_STAT_TX_BUS_ANOMALY_DETECTED),
			cec->cec_base + TEGRA_CEC_INT_STAT);
	} else if (status & TEGRA_CEC_INT_STAT_TX_REGISTER_EMPTY) {
		cec->tx_wake = 1;
		wake_up_interruptible(&cec->tx_waitq);
		writel((TEGRA_CEC_INT_STAT_TX_REGISTER_EMPTY),
		   cec->cec_base + TEGRA_CEC_INT_STAT);
	} else if (status & TEGRA_CEC_INT_STAT_TX_FRAME_TRANSMITTED) {
		writel((TEGRA_CEC_INT_STAT_TX_FRAME_TRANSMITTED),
		   cec->cec_base + TEGRA_CEC_INT_STAT);
	}

	return IRQ_HANDLED;
}

static const struct file_operations tegra_cec_fops = {
	.owner = THIS_MODULE,
	.open = tegra_cec_open,
	.release = tegra_cec_release,
	.read = tegra_cec_read,
	.write = tegra_cec_write,
};

static void tegra_cec_init(struct tegra_cec *cec)
{

	dev_notice(cec->dev, "%s started\n", __func__);

	writel(0x00, cec->cec_base + TEGRA_CEC_HW_CONTROL);
	writel(0x00, cec->cec_base + TEGRA_CEC_INT_MASK);
	writel(0xffffffff, cec->cec_base + TEGRA_CEC_INT_STAT);
	msleep(1000);

	writel(0x00, cec->cec_base + TEGRA_CEC_SW_CONTROL);

	writel((
	   (cec->logical_addr << TEGRA_CEC_HW_CONTROL_RX_LOGICAL_ADDRS_MASK) &
	   (~TEGRA_CEC_HW_CONTROL_RX_SNOOP) &
	   (~TEGRA_CEC_HW_CONTROL_RX_NAK_MODE) &
	   (~TEGRA_CEC_HW_CONTROL_TX_NAK_MODE) &
	   (~TEGRA_CEC_HW_CONTROL_FAST_SIM_MODE)) |
	   (TEGRA_CEC_HW_CONTROL_TX_RX_MODE),
	   cec->cec_base + TEGRA_CEC_HW_CONTROL);

	writel(0x00, cec->cec_base + TEGRA_CEC_INPUT_FILTER);

	writel((0x7a << TEGRA_CEC_RX_TIMING_0_RX_START_BIT_MAX_LO_TIME_MASK) |
	   (0x6d << TEGRA_CEC_RX_TIMING_0_RX_START_BIT_MIN_LO_TIME_MASK) |
	   (0x93 << TEGRA_CEC_RX_TIMING_0_RX_START_BIT_MAX_DURATION_MASK) |
	   (0x86 << TEGRA_CEC_RX_TIMING_0_RX_START_BIT_MIN_DURATION_MASK),
	   cec->cec_base + TEGRA_CEC_RX_TIMING_0);

	writel((0x35 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_MAX_LO_TIME_MASK) |
	   (0x21 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_SAMPLE_TIME_MASK) |
	   (0x56 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_MAX_DURATION_MASK) |
	   (0x40 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_MIN_DURATION_MASK),
	   cec->cec_base + TEGRA_CEC_RX_TIMING_1);

	writel((0x50 << TEGRA_CEC_RX_TIMING_2_RX_END_OF_BLOCK_TIME_MASK),
	   cec->cec_base + TEGRA_CEC_RX_TIMING_2);

	writel((0x74 << TEGRA_CEC_TX_TIMING_0_TX_START_BIT_LO_TIME_MASK) |
	   (0x8d << TEGRA_CEC_TX_TIMING_0_TX_START_BIT_DURATION_MASK) |
	   (0x08 << TEGRA_CEC_TX_TIMING_0_TX_BUS_XITION_TIME_MASK) |
	   (0x71 << TEGRA_CEC_TX_TIMING_0_TX_BUS_ERROR_LO_TIME_MASK),
	   cec->cec_base + TEGRA_CEC_TX_TIMING_0);

	writel((0x2f << TEGRA_CEC_TX_TIMING_1_TX_LO_DATA_BIT_LO_TIME_MASK) |
	   (0x13 << TEGRA_CEC_TX_TIMING_1_TX_HI_DATA_BIT_LO_TIME_MASK) |
	   (0x4b << TEGRA_CEC_TX_TIMING_1_TX_DATA_BIT_DURATION_MASK) |
	   (0x21 << TEGRA_CEC_TX_TIMING_1_TX_ACK_NAK_BIT_SAMPLE_TIME_MASK),
	   cec->cec_base + TEGRA_CEC_TX_TIMING_1);

	writel((0x07 << TEGRA_CEC_TX_TIMING_2_BUS_IDLE_TIME_ADDITIONAL_FRAME_MASK) |
	   (0x05 << TEGRA_CEC_TX_TIMING_2_TX_BUS_IDLE_TIME_NEW_FRAME_MASK) |
	   (0x03 << TEGRA_CEC_TX_TIMING_2_TX_BUS_IDLE_TIME_RETRY_FRAME_MASK),
	   cec->cec_base + TEGRA_CEC_TX_TIMING_2);

	writel((TEGRA_CEC_INT_MASK_TX_REGISTER_UNDERRUN |
	    TEGRA_CEC_INT_MASK_TX_FRAME_OR_BLOCK_NAKD |
	    TEGRA_CEC_INT_MASK_TX_ARBITRATION_FAILED |
	    TEGRA_CEC_INT_MASK_TX_BUS_ANOMALY_DETECTED |
	    TEGRA_CEC_INT_MASK_RX_REGISTER_FULL |
	    TEGRA_CEC_INT_MASK_RX_REGISTER_OVERRUN),
	   cec->cec_base + TEGRA_CEC_INT_MASK);

	atomic_set(&cec->init_done, 1);
	wake_up_interruptible(&cec->init_waitq);

	dev_notice(cec->dev, "%s Done.\n", __func__);
}

static void tegra_cec_init_worker(struct work_struct *work)
{
	struct tegra_cec *cec = container_of(work, struct tegra_cec, work);

	tegra_cec_init(cec);
}

static ssize_t cec_logical_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tegra_cec *cec = dev_get_drvdata(dev);

	if (buf)
		return sprintf(buf, "0x%x\n", (u32)cec->logical_addr);
	return 1;
}

static ssize_t cec_logical_addr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret;
	u32 state;
	u16 addr;
	struct tegra_cec *cec;

	if (!buf || !count)
		return -EINVAL;

	cec = dev_get_drvdata(dev);
	if (!atomic_read(&cec->init_done))
		return -EAGAIN;

	ret = kstrtou16(buf, 0, &addr);
	if (ret)
		return ret;


	dev_info(dev, "tegra_cec: set logical address: %x\n", (u32)addr);
	cec->logical_addr = addr;
	state = readl(cec->cec_base + TEGRA_CEC_HW_CONTROL);
	state &= ~TEGRA_CEC_HWCTRL_RX_LADDR_MASK;
	state |= TEGRA_CEC_HWCTRL_RX_LADDR(cec->logical_addr);
	writel(state, cec->cec_base + TEGRA_CEC_HW_CONTROL);

	return count;
}

static int tegra_cec_probe(struct platform_device *pdev)
{
	struct tegra_cec *cec;
	struct resource *res;
	int ret = 0;

	cec = devm_kzalloc(&pdev->dev, sizeof(struct tegra_cec), GFP_KERNEL);

	if (!cec)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		dev_err(&pdev->dev,
		    "Unable to allocate resources for device.\n");
		ret = -EBUSY;
		goto cec_error;
	}

	if (!devm_request_mem_region(&pdev->dev, res->start, resource_size(res),
		pdev->name)) {
		dev_err(&pdev->dev,
			"Unable to request mem region for device.\n");
		ret = -EBUSY;
		goto cec_error;
	}

	cec->tegra_cec_irq = platform_get_irq(pdev, 0);

	if (cec->tegra_cec_irq <= 0) {
		ret = -EBUSY;
		goto cec_error;
	}

	cec->cec_base = devm_ioremap_nocache(&pdev->dev, res->start,
		resource_size(res));

	if (!cec->cec_base) {
		dev_err(&pdev->dev, "Unable to grab IOs for device.\n");
		ret = -EBUSY;
		goto cec_error;
	}

	atomic_set(&cec->init_done, 0);
	cec->logical_addr = TEGRA_CEC_LOGICAL_ADDR;

	cec->clk = clk_get(&pdev->dev, "cec");

	if (IS_ERR_OR_NULL(cec->clk)) {
		dev_err(&pdev->dev, "can't get clock for CEC\n");
		ret = -ENOENT;
		goto clk_error;
	}

	clk_enable(cec->clk);

	/* set context info. */
	cec->dev = &pdev->dev;
	cec->rx_wake = 0;
	cec->tx_wake = 0;
	init_waitqueue_head(&cec->rx_waitq);
	init_waitqueue_head(&cec->tx_waitq);
	init_waitqueue_head(&cec->init_waitq);

	platform_set_drvdata(pdev, cec);
	/* clear out the hardware. */

	INIT_WORK(&cec->work, tegra_cec_init_worker);
	schedule_work(&cec->work);

	device_init_wakeup(&pdev->dev, 1);

	cec->misc_dev.minor = MISC_DYNAMIC_MINOR;
	cec->misc_dev.name = TEGRA_CEC_NAME;
	cec->misc_dev.fops = &tegra_cec_fops;
	cec->misc_dev.parent = &pdev->dev;

	if (misc_register(&cec->misc_dev)) {
		printk(KERN_WARNING "Couldn't register device , %s.\n", TEGRA_CEC_NAME);
		goto cec_error;
	}

	ret = devm_request_irq(&pdev->dev, cec->tegra_cec_irq,
		tegra_cec_irq_handler, IRQF_DISABLED, "cec_irq", &pdev->dev);

	if (ret) {
		dev_err(&pdev->dev,
			"Unable to request interrupt for device (err=%d).\n", ret);
		goto cec_error;
	}

	ret = sysfs_create_file(
		&pdev->dev.kobj, &dev_attr_cec_logical_addr_config.attr);
	dev_info(&pdev->dev, "cec_add_sysfs ret=%d\n", ret);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to add sysfs: %d\n", ret);
		goto cec_error;
	}

	dev_notice(&pdev->dev, "probed\n");

	return 0;

cec_error:
	clk_disable(cec->clk);
	clk_put(cec->clk);
clk_error:
	return ret;
}

static int tegra_cec_remove(struct platform_device *pdev)
{
	struct tegra_cec *cec = platform_get_drvdata(pdev);

	clk_disable(cec->clk);
	clk_put(cec->clk);

	misc_deregister(&cec->misc_dev);
	cancel_work_sync(&cec->work);

	return 0;
}

#ifdef CONFIG_PM
static int tegra_cec_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tegra_cec *cec = platform_get_drvdata(pdev);

	/* cancel the work queue */
	cancel_work_sync(&cec->work);

	atomic_set(&cec->init_done, 0);

	clk_disable(cec->clk);

	dev_notice(&pdev->dev, "suspended\n");
	return 0;
}

static int tegra_cec_resume(struct platform_device *pdev)
{
	struct tegra_cec *cec = platform_get_drvdata(pdev);

	dev_notice(&pdev->dev, "Resuming\n");

	clk_enable(cec->clk);
	schedule_work(&cec->work);

	return 0;
}
#endif

static struct platform_driver tegra_cec_driver = {
	.driver = {
		.name = TEGRA_CEC_NAME,
		.owner = THIS_MODULE,
	},
	.probe = tegra_cec_probe,
	.remove = tegra_cec_remove,

#ifdef CONFIG_PM
	.suspend = tegra_cec_suspend,
	.resume = tegra_cec_resume,
#endif
};

module_platform_driver(tegra_cec_driver);
