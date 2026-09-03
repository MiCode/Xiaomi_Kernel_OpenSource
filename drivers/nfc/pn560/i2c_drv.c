/******************************************************************************
 * Copyright (C) 2015, The Linux Foundation. All rights reserved.
 * Copyright 2013-2022, 2024 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ******************************************************************************/
/*
 * Copyright (C) 2010 Trusted Logic S.A.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include "common.h"
#include <linux/clk.h>
#include <linux/regmap.h> 
#include <linux/of_platform.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include "common_ese.h"
#include "nfc_vbat_monitor.h"
#if IS_ENABLED(CONFIG_NXP_COLD_RESET)
#include "cold_reset.h"
#include <linux/kthread.h>             //kernel threads
#endif

#define NFC_CLK_FREQ 26000000
static struct device_node *np = NULL;


int pmic_refout_update(struct sec_nfc_info *info, unsigned int refout_num, int refout_state)
{
    int ret;
    if (info->sc27xx_refout == NULL || refout_num >= info->sc27xx_refout->refnum) {
        pr_err("%s- sc27xx_refout struct is invalid\n", __func__);
        return -EINVAL;
    }

    if (!refout_state)
        ret = regmap_update_bits(info->sc27xx_refout->regmap, info->sc27xx_refout->regsw,
                 1 << refout_num, 0);
    else if (refout_state == 1)
        ret = regmap_update_bits(info->sc27xx_refout->regmap, info->sc27xx_refout->regsw,
                 1 << refout_num, 1 << refout_num);
    else {
        pr_err("Invalid state(%d)\n", refout_state);
        ret = -EINVAL;
    }

    return ret;
}
/**
 * i2c_disable_irq()
 *
 * Check if interrupt is disabled or not
 * and disable interrupt
 *
 * Return: int
 */
int i2c_disable_irq(struct nfc_dev *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->i2c_dev.irq_enabled_lock, flags);
	if (dev->i2c_dev.irq_enabled) {
		disable_irq_nosync(dev->i2c_dev.client->irq);
		dev->i2c_dev.irq_enabled = false;
	}
	spin_unlock_irqrestore(&dev->i2c_dev.irq_enabled_lock, flags);

	return 0;
}

/**
 * i2c_enable_irq()
 *
 * Check if interrupt is enabled or not
 * and enable interrupt
 *
 * Return: int
 */
int i2c_enable_irq(struct nfc_dev *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->i2c_dev.irq_enabled_lock, flags);
	if (!dev->i2c_dev.irq_enabled) {
		dev->i2c_dev.irq_enabled = true;
		enable_irq(dev->i2c_dev.client->irq);
	}
	spin_unlock_irqrestore(&dev->i2c_dev.irq_enabled_lock, flags);

	return 0;
}

static irqreturn_t i2c_irq_handler(int irq, void *dev_id)
{
	struct nfc_dev *nfc_dev = dev_id;
	struct i2c_dev *i2c_dev = &nfc_dev->i2c_dev;

	if (device_may_wakeup(&i2c_dev->client->dev))
		pm_wakeup_event(&i2c_dev->client->dev, WAKEUP_SRC_TIMEOUT);

	i2c_disable_irq(nfc_dev);
	wake_up(&nfc_dev->read_wq);

	return IRQ_HANDLED;
}

int i2c_read(struct nfc_dev *nfc_dev, char *buf, size_t count, int timeout)
{
	int ret;
	struct i2c_dev *i2c_dev = &nfc_dev->i2c_dev;
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	pr_debug("%s: reading %zu bytes.\n", __func__, count);

	if (timeout > NCI_CMD_RSP_TIMEOUT_MS)
		timeout = NCI_CMD_RSP_TIMEOUT_MS;

	if (count > MAX_NCI_BUFFER_SIZE)
		count = MAX_NCI_BUFFER_SIZE;

	if (!gpio_get_value(nfc_gpio->irq)) {
		while (1) {
			ret = 0;
			if (!i2c_dev->irq_enabled) {
				i2c_dev->irq_enabled = true;
				enable_irq(i2c_dev->client->irq);
			}
			if (!gpio_get_value(nfc_gpio->irq)) {
				if (timeout) {
					ret = wait_event_interruptible_timeout(
						nfc_dev->read_wq,
						!i2c_dev->irq_enabled,
						msecs_to_jiffies(timeout));
					if (ret <= 0) {
						pr_err("%s: timeout error\n",
						       __func__);
						goto err;
					}
				} else {
					ret = wait_event_interruptible(
						nfc_dev->read_wq,
						!i2c_dev->irq_enabled);
					if (ret) {
						pr_err("%s: err wakeup of wq\n",
						       __func__);
						goto err;
					}
				}
			}
#if IS_ENABLED(CONFIG_NXP_NFC_VBAT_MONITOR)
			if (nfc_dev->nfc_vbat_monitor.vbat_monitor_status) {
				pr_debug("%s: NFC recovering  state\n",
					 __func__);
				nfc_dev->nfc_vbat_monitor.vbat_monitor_status =
					false;
				ret = -ENOTCONN;
				goto err;
			}
#endif /* CONFIG_NXP_NFC_VBAT_MONITOR */
			i2c_disable_irq(nfc_dev);

			if (gpio_get_value(nfc_gpio->irq))
				break;
			if (!gpio_get_value(nfc_gpio->ven)) {
				pr_info("%s: releasing read\n", __func__);
				ret = -EIO;
				goto err;
			}
			/*
			 * NFC service wanted to close the driver so,
			 * release the calling reader thread asap.
			 *
			 * This can happen in case of nfc node close call from
			 * eSE HAL in that case the NFC HAL reader thread
			 * will again call read system call
			 */
			if (nfc_dev->release_read) {
				pr_debug("%s: releasing read\n", __func__);
				return 0;
			}
			pr_warn("%s: spurious interrupt detected\n", __func__);
		}
	}

	memset(buf, 0x00, count);
	/* Read data */
	ret = i2c_master_recv(nfc_dev->i2c_dev.client, buf, count);
	if (ret <= 0) {
		pr_err("%s: returned %d\n", __func__, ret);
		goto err;
	}
	/* check if it's response of cold reset command
	 * NFC HAL process shouldn't receive this data as
	 * command was sent by driver
	 */
	if (nfc_dev->cold_reset.rsp_pending) {
		if (IS_PROP_CMD_RSP(buf)) {
			/* Read data */
			ret = i2c_master_recv(nfc_dev->i2c_dev.client,
					      &buf[NCI_PAYLOAD_IDX],
					      buf[NCI_PAYLOAD_LEN_IDX]);
			if (ret <= 0) {
				pr_err("%s: error reading cold rst/prot rsp\n",
				       __func__);
				goto err;
			}
			wakeup_on_prop_rsp(nfc_dev, buf);
			/*
			 * NFC process doesn't know about cold reset command
			 * being sent as it was initiated by eSE process
			 * we shouldn't return any data to NFC process
			 */
			return 0;
		}
	}
err:
	return ret;
}

int i2c_write(struct nfc_dev *nfc_dev, const char *buf, size_t count,
	      int max_retry_cnt)
{
	int ret = -EINVAL;
	int retry_cnt;
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	if (count > MAX_DL_BUFFER_SIZE)
		count = MAX_DL_BUFFER_SIZE;

	pr_debug("%s: writing %zu bytes.\n", __func__, count);
	/*
	 * Wait for any pending read for max 15ms before write
	 * This is to avoid any packet corruption during read, when
	 * the host cmds resets NFCC during any parallel read operation
	 */
	for (retry_cnt = 1; retry_cnt <= MAX_WRITE_IRQ_COUNT; retry_cnt++) {
		if (gpio_get_value(nfc_gpio->irq)) {
			pr_warn("%s: irq high during write, wait\n", __func__);
			usleep_range(WRITE_RETRY_WAIT_TIME_US,
				     WRITE_RETRY_WAIT_TIME_US + 100);
		} else {
			break;
		}
		if (retry_cnt == MAX_WRITE_IRQ_COUNT &&
		    gpio_get_value(nfc_gpio->irq)) {
			pr_warn("%s: allow after maximum wait\n", __func__);
		}
	}

	for (retry_cnt = 1; retry_cnt <= max_retry_cnt; retry_cnt++) {
		ret = i2c_master_send(nfc_dev->i2c_dev.client, buf, count);
		if (ret <= 0) {
			pr_warn("%s: write failed ret(%d), maybe in standby\n",
				__func__, ret);
			usleep_range(WRITE_RETRY_WAIT_TIME_US,
				     WRITE_RETRY_WAIT_TIME_US + 100);
		} else if (ret != count) {
			pr_err("%s: failed to write %d\n", __func__, ret);
			ret = -EIO;
		} else if (ret == count)
			break;
	}
	return ret;
}

ssize_t nfc_i2c_dev_read(struct file *filp, char __user *buf, size_t count,
			 loff_t *offset)
{
	int ret;
	struct nfc_dev *nfc_dev = (struct nfc_dev *)filp->private_data;

	if (!nfc_dev) {
		pr_err("%s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	mutex_lock(&nfc_dev->read_mutex);
	if (filp->f_flags & O_NONBLOCK) {
		ret = i2c_master_recv(nfc_dev->i2c_dev.client,
				      nfc_dev->read_kbuf, count);
		pr_debug("%s: NONBLOCK read ret = %d\n", __func__, ret);
	} else {
		ret = i2c_read(nfc_dev, nfc_dev->read_kbuf, count, 0);
	}
	if (ret > 0) {
		if (copy_to_user(buf, nfc_dev->read_kbuf, ret)) {
			pr_warn("%s: failed to copy to user space\n", __func__);
			ret = -EFAULT;
		}
	}
	mutex_unlock(&nfc_dev->read_mutex);
	return ret;
}

ssize_t nfc_i2c_dev_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *offset)
{
	int ret;
	struct nfc_dev *nfc_dev = (struct nfc_dev *)filp->private_data;

	if (count > MAX_DL_BUFFER_SIZE)
		count = MAX_DL_BUFFER_SIZE;

	if (!nfc_dev) {
		pr_err("%s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&nfc_dev->write_mutex);
	if (copy_from_user(nfc_dev->write_kbuf, buf, count)) {
		pr_err("%s: failed to copy from user space\n", __func__);
		mutex_unlock(&nfc_dev->write_mutex);
		return -EFAULT;
	}
	ret = i2c_write(nfc_dev, nfc_dev->write_kbuf, count, NO_RETRY);
	mutex_unlock(&nfc_dev->write_mutex);
	return ret;
}

static const struct file_operations nfc_i2c_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = nfc_i2c_dev_read,
	.write = nfc_i2c_dev_write,
	.open = nfc_dev_open,
	.flush = nfc_dev_flush,
	.release = nfc_dev_close,
	.unlocked_ioctl = nfc_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = nfc_dev_compat_ioctl,
#endif
};

int nfc_i2c_dev_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        int ret = 0;
        struct nfc_dev *nfc_dev = NULL;
        struct i2c_dev *i2c_dev = NULL;
        struct platform_configs *nfc_configs = NULL;
        struct platform_gpio *nfc_gpio = NULL;
        struct platform_device *pmic_device = NULL;
        unsigned int reg_val = 0;
        struct sec_nfc_info *info;
          
	pr_debug("%s: enter\n", __func__);
	nfc_dev = kzalloc(sizeof(struct nfc_dev), GFP_KERNEL);
	if (nfc_dev == NULL) {
		ret = -ENOMEM;
		goto err;
	}

        info = &nfc_dev->nfc_info;
        info->dev = &client->dev;  
  
    np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-refout");
    if (!np) {
        pr_err("devm_clk_get nfc_clk\n");
        goto err_free_refout;
    } else {
        pr_debug("%s- enter refout init.\n", __func__);

        info->sc27xx_refout = kzalloc(sizeof(struct pmic_refout), GFP_KERNEL);
        if (!info->sc27xx_refout) {
            pr_err("%s- can't alloc memory for pmic refout", __func__);
            ret = -ENOMEM;
            goto err_free_nfc_dev;
        }

        ret = of_property_read_u32_index(np, "regsw", 0, &info->sc27xx_refout->regsw);
        if (ret) {
            pr_err("Get base register failed\n");
            goto err_free_refout;
        }

        ret = of_property_read_u32_index(np, "refnum", 0, &info->sc27xx_refout->refnum);
        if (ret) {
            pr_err("Get refout num failed\n");
            goto err_free_refout;
        }
        
        pmic_device = of_find_device_by_node(np);
        if (!pmic_device) {
            of_node_put(np);
            pr_err("%s- find refout node fail.\n", __func__);
            ret = -ENODEV;
            goto err_free_refout;
        }
        of_node_put(np);
        
        info->sc27xx_refout->regmap = dev_get_regmap(pmic_device->dev.parent, NULL);
        if (!info->sc27xx_refout->regmap) {
            put_device(&pmic_device->dev);
            pr_err("%s- can't get pmic regmap device\n", __func__);
            ret = -ENODEV;
            goto err_free_refout;
        }
        put_device(&pmic_device->dev);
        
        ret = regmap_read(info->sc27xx_refout->regmap, 
                         info->sc27xx_refout->regsw, 
                         &reg_val);
        if (ret) {
            pr_err("Regmap read failed: %d\n", ret);
            goto err_free_refout;
        }
        pr_debug("%s- reg_val value = %u", __func__, reg_val);
    }
   
    if (info->clk_26m) {
        ret = clk_prepare_enable(info->clk_26m);
        if (ret) {
            dev_err(&client->dev, "clk_26m enable failed\n");
            goto err_free_refout;
        }
    }
    
        if (np) {
            ret = pmic_refout_update(info, 4, 1);
            if (ret) {
                dev_err(&client->dev, "Failed to enable REF_OUT4 output\n");
                goto err_free_refout;
            }
        }
        dev_err(&client->dev, "Enabled REF_OUT4 for NFC 26MHz clock\n");
  
	nfc_configs = &nfc_dev->configs;
	nfc_gpio = &nfc_configs->gpio;
	/* retrieve details of gpios from dt */
	ret = nfc_parse_dt(&client->dev, nfc_configs, PLATFORM_IF_I2C);
	if (ret) {
		pr_err("%s: failed to parse dt\n", __func__);
		goto err_free_nfc_dev;
	}
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto err_free_nfc_dev;
	}
	nfc_dev->read_kbuf = kzalloc(MAX_NCI_BUFFER_SIZE, GFP_DMA | GFP_KERNEL);
	if (!nfc_dev->read_kbuf) {
		ret = -ENOMEM;
		goto err_free_nfc_dev;
	}
	nfc_dev->write_kbuf = kzalloc(MAX_DL_BUFFER_SIZE, GFP_DMA | GFP_KERNEL);
	if (!nfc_dev->write_kbuf) {
		ret = -ENOMEM;
		goto err_free_read_kbuf;
	}
	nfc_dev->interface = PLATFORM_IF_I2C;
	nfc_dev->nfc_state = NFC_STATE_NCI;
	nfc_dev->i2c_dev.client = client;
	i2c_dev = &nfc_dev->i2c_dev;
	nfc_dev->nfc_read = i2c_read;
	nfc_dev->nfc_write = i2c_write;
	nfc_dev->nfc_enable_intr = i2c_enable_irq;
	nfc_dev->nfc_disable_intr = i2c_disable_irq;
	ret = configure_gpio(nfc_gpio->ven, GPIO_OUTPUT);
	if (ret) {
		pr_err("%s: unable to request nfc reset gpio [%d]\n", __func__,
		       nfc_gpio->ven);
		goto err_free_write_kbuf;
	}
	ret = configure_gpio(nfc_gpio->irq, GPIO_IRQ);
	if (ret <= 0) {
		pr_err("%s: unable to request nfc irq gpio [%d]\n", __func__,
		       nfc_gpio->irq);
		goto err_free_gpio;
	}
	client->irq = ret;
	ret = configure_gpio(nfc_gpio->dwl_req, GPIO_OUTPUT);
	if (ret) {
		pr_err("%s: unable to request nfc firm downl gpio [%d]\n",
		       __func__, nfc_gpio->dwl_req);
	}
	/* init mutex and queues */
	init_waitqueue_head(&nfc_dev->read_wq);
	mutex_init(&nfc_dev->read_mutex);
	mutex_init(&nfc_dev->write_mutex);
	mutex_init(&nfc_dev->dev_ref_mutex);
	spin_lock_init(&i2c_dev->irq_enabled_lock);
	common_ese_init(nfc_dev);
	ret = nfc_misc_register(nfc_dev, &nfc_i2c_dev_fops, DEV_COUNT,
				NFC_CHAR_DEV_NAME, CLASS_NAME);
	if (ret) {
		pr_err("%s: nfc_misc_register failed\n", __func__);
		goto err_mutex_destroy;
	}
	/* interrupt initializations */
	pr_info("%s: requesting IRQ %d\n", __func__, client->irq);
	i2c_dev->irq_enabled = true;
	ret = request_irq(client->irq, i2c_irq_handler, IRQF_TRIGGER_HIGH,
			  client->name, nfc_dev);
	if (ret) {
		pr_err("%s: request_irq failed\n", __func__);
		goto err_nfc_misc_unregister;
	}
#if IS_ENABLED(CONFIG_NXP_NFC_VBAT_MONITOR)
	ret = nfc_vbat_monitor_init(nfc_dev, nfc_gpio, client);
	if (ret) {
		pr_err("%s: nfcc vbat monitor init failed, ret: %d\n", __func__, ret);
		goto err_nfc_misc_unregister;
	}
#endif /* CONFIG_NXP_NFC_VBAT_MONITOR */
	i2c_disable_irq(nfc_dev);
	gpio_set_ven(nfc_dev, 1);
	gpio_set_ven(nfc_dev, 0);
	gpio_set_ven(nfc_dev, 1);
	device_init_wakeup(&client->dev, true);
	i2c_set_clientdata(client, nfc_dev);
	i2c_dev->irq_wake_up = false;

	pr_info("%s: probing nfc i2c successfully\n", __func__);
#if IS_ENABLED(CONFIG_NXP_COLD_RESET)
	nfc_dev->release_read = false;
	ret = nfc_dev_func(nfc_dev);
	etx_thread = kthread_run(cold_reset_thread_handler, NULL, "eTx Thread");
	if (etx_thread) {
		pr_info("Kthread Created Successfully...\n");
	} else {
		pr_info("Cannot create kthread\n");
	}
#endif

	return 0;
err_free_refout:
    if (info->sc27xx_refout){
        if (info->clk_enable){
            clk_disable_unprepare(info->clk_enable);}
        if (info->clk_26m){
            clk_disable_unprepare(info->clk_26m);}
        kfree(info->sc27xx_refout);
    }
err_nfc_misc_unregister:
	nfc_misc_unregister(nfc_dev, DEV_COUNT);
err_mutex_destroy:
	mutex_destroy(&nfc_dev->dev_ref_mutex);
	mutex_destroy(&nfc_dev->read_mutex);
	mutex_destroy(&nfc_dev->write_mutex);
err_free_gpio:
	gpio_free_all(nfc_dev);
err_free_write_kbuf:
	kfree(nfc_dev->write_kbuf);
err_free_read_kbuf:
	kfree(nfc_dev->read_kbuf);
err_free_nfc_dev:
	kfree(nfc_dev);
err:
	pr_err("%s: probing not successful, check hardware\n", __func__);
	return ret;
}

int nfc_i2c_dev_remove(struct i2c_client *client)
{
	int ret = 0;
	struct nfc_dev *nfc_dev = NULL;
	struct sec_nfc_info *info;  

	pr_info("%s: remove device\n", __func__);
	nfc_dev = i2c_get_clientdata(client);
	if (!nfc_dev) {
		pr_err("%s: device doesn't exist anymore\n", __func__);
		ret = -ENODEV;
		return ret;
	}
	info = &nfc_dev->nfc_info;
	if (nfc_dev->dev_ref_count > 0) {
		pr_err("%s: device already in use\n", __func__);
		return -EBUSY;
	}

    if (info->clk_26m) {
        clk_disable_unprepare(info->clk_26m);
    }
    if (info->clk_enable) {
        clk_disable_unprepare(info->clk_enable);
    }
    if (info->sc27xx_refout) {
        kfree(info->sc27xx_refout);
    }
#if IS_ENABLED(CONFIG_NXP_COLD_RESET)
	nfc_dev_cold_reset_flush();
#endif
	device_init_wakeup(&client->dev, false);
	free_irq(client->irq, nfc_dev);
#if IS_ENABLED(CONFIG_NXP_NFC_VBAT_MONITOR)
	free_irq(nfc_dev->nfc_vbat_monitor.irq_num, nfc_dev);
#endif /* CONFIG_NXP_NFC_VBAT_MONITOR */
	nfc_misc_unregister(nfc_dev, DEV_COUNT);
	mutex_destroy(&nfc_dev->read_mutex);
	mutex_destroy(&nfc_dev->write_mutex);
	gpio_free_all(nfc_dev);
	kfree(nfc_dev->read_kbuf);
	kfree(nfc_dev->write_kbuf);
	kfree(nfc_dev);
	return ret;
}

int nfc_i2c_dev_suspend(struct device *device)
{
	struct i2c_client *client = to_i2c_client(device);
	struct nfc_dev *nfc_dev = i2c_get_clientdata(client);
	struct i2c_dev *i2c_dev = NULL;

	if (!nfc_dev) {
		pr_err("%s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	i2c_dev = &nfc_dev->i2c_dev;

	if (device_may_wakeup(&client->dev) && i2c_dev->irq_enabled) {
		if (!enable_irq_wake(client->irq))
			i2c_dev->irq_wake_up = true;
	}
#if IS_ENABLED(CONFIG_NXP_NFC_VBAT_MONITOR)
	if (enable_irq_wake(nfc_dev->nfc_vbat_monitor.irq_num) != 0)
		pr_err("%s: vbat irq wake enabled failed\n", __func__);
#endif /* CONFIG_NXP_NFC_VBAT_MONITOR */
	pr_debug("%s: irq_wake_up = %d", __func__, i2c_dev->irq_wake_up);
	return 0;
}

int nfc_i2c_dev_resume(struct device *device)
{
	struct i2c_client *client = to_i2c_client(device);
	struct nfc_dev *nfc_dev = i2c_get_clientdata(client);
	struct i2c_dev *i2c_dev = NULL;

	if (!nfc_dev) {
		pr_err("%s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	i2c_dev = &nfc_dev->i2c_dev;

	if (device_may_wakeup(&client->dev) && i2c_dev->irq_wake_up) {
		if (!disable_irq_wake(client->irq))
			i2c_dev->irq_wake_up = false;
	}
#if IS_ENABLED(CONFIG_NXP_NFC_VBAT_MONITOR)
	if (disable_irq_wake(nfc_dev->nfc_vbat_monitor.irq_num) != 0)
		pr_err("%s: vbat irq wake disabled failed\n", __func__);
#endif /* CONFIG_NXP_NFC_VBAT_MONITOR */
	pr_debug("%s: irq_wake_up = %d", __func__, i2c_dev->irq_wake_up);
	return 0;
}

static const struct i2c_device_id nfc_i2c_dev_id[] = { { NFC_I2C_DEV_ID, 0 },
						       {} };

static const struct of_device_id nfc_i2c_dev_match_table[] = {
	{
		.compatible = NFC_I2C_DRV_STR,
	},
	{}
};

static const struct dev_pm_ops nfc_i2c_dev_pm_ops = { SET_SYSTEM_SLEEP_PM_OPS(
	nfc_i2c_dev_suspend, nfc_i2c_dev_resume) };

static struct i2c_driver nfc_i2c_dev_driver = {
	.id_table = nfc_i2c_dev_id,
	.probe = nfc_i2c_dev_probe,
	.remove = nfc_i2c_dev_remove,
	.driver = {
		.name = NFC_I2C_DRV_STR,
		.pm = &nfc_i2c_dev_pm_ops,
		.of_match_table = nfc_i2c_dev_match_table,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

MODULE_DEVICE_TABLE(of, nfc_i2c_dev_match_table);

static int __init nfc_i2c_dev_init(void)
{
	int ret = 0;

	pr_info("%s: Loading NXP NFC I2C driver\n", __func__);
	ret = i2c_add_driver(&nfc_i2c_dev_driver);
	if (ret != 0)
		pr_err("%s: NFC I2C add driver error ret %d\n", __func__, ret);
	return ret;
}

module_init(nfc_i2c_dev_init);

static void __exit nfc_i2c_dev_exit(void)
{
	pr_info("%s: Unloading NXP NFC I2C driver\n", __func__);
	i2c_del_driver(&nfc_i2c_dev_driver);
}

module_exit(nfc_i2c_dev_exit);

MODULE_DESCRIPTION("NXP NFC I2C driver");
MODULE_LICENSE("GPL");
