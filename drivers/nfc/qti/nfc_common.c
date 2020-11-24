// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include "nfc_common.h"


int nfc_parse_dt(struct device *dev, struct platform_gpio *nfc_gpio,
		 struct platform_ldo *ldo, uint8_t interface)
{
	struct device_node *np = dev->of_node;
	int ret;

	if (!np) {
		pr_err("nfc of_node NULL\n");
		return -EINVAL;
	}

	if (interface == PLATFORM_IF_I2C) {
		nfc_gpio->irq = of_get_named_gpio(np, DTS_IRQ_GPIO_STR, 0);
		if ((!gpio_is_valid(nfc_gpio->irq))) {
			pr_err("nfc irq gpio invalid %d\n", nfc_gpio->irq);
			return -EINVAL;
		}
		pr_info("%s: irq %d\n", __func__, nfc_gpio->irq);
	}

	nfc_gpio->ven = of_get_named_gpio(np, DTS_VEN_GPIO_STR, 0);
	if ((!gpio_is_valid(nfc_gpio->ven))) {
		pr_err("nfc ven gpio invalid %d\n", nfc_gpio->ven);
		return -EINVAL;
	}

	nfc_gpio->dwl_req = of_get_named_gpio(np, DTS_FWDN_GPIO_STR, 0);
	if ((!gpio_is_valid(nfc_gpio->dwl_req))) {
		pr_err("nfc dwl_req gpio invalid %d\n", nfc_gpio->dwl_req);
		return -EINVAL;
	}

	nfc_gpio->clkreq = of_get_named_gpio(np, DTS_CLKREQ_GPIO_STR, 0);
	if (!gpio_is_valid(nfc_gpio->clkreq)) {
		dev_err(dev, "clkreq gpio invalid %d\n", nfc_gpio->dwl_req);
		return -EINVAL;
	}

	pr_info("%s: ven %d, dwl req %d, clkreq %d\n", __func__,
		nfc_gpio->ven, nfc_gpio->dwl_req, nfc_gpio->clkreq);

	// optional property
	ret = of_property_read_u32_array(np, NFC_LDO_VOL_DT_NAME,
			(u32 *) ldo->vdd_levels,
			ARRAY_SIZE(ldo->vdd_levels));
	if (ret) {
		dev_err(dev, "error reading NFC VDDIO min and max value\n");
		// set default as per datasheet
		ldo->vdd_levels[0] = NFC_VDDIO_MIN;
		ldo->vdd_levels[1] = NFC_VDDIO_MAX;
	}

	// optional property
	ret = of_property_read_u32(np, NFC_LDO_CUR_DT_NAME, &ldo->max_current);
	if (ret) {
		dev_err(dev, "error reading NFC current value\n");
		// set default as per datasheet
		ldo->max_current = NFC_CURRENT_MAX;
	}

	return 0;
}

/**
 * nfc_ldo_vote()
 * @nfc_dev: NFC device containing regulator handle
 *
 * LDO voting based on voltage and current entries in DT
 *
 * Return: 0 on success and -ve on failure
 */
int nfc_ldo_vote(struct nfc_dev *nfc_dev)
{
	int ret;

	ret =  regulator_set_voltage(nfc_dev->reg,
			nfc_dev->ldo.vdd_levels[0],
			nfc_dev->ldo.vdd_levels[1]);
	if (ret < 0) {
		pr_err("%s: set voltage failed\n", __func__);
		return ret;
	}

	/* pass expected current from NFC in uA */
	ret = regulator_set_load(nfc_dev->reg, nfc_dev->ldo.max_current);
	if (ret < 0) {
		pr_err("%s: set load failed\n", __func__);
		return ret;
	}

	ret = regulator_enable(nfc_dev->reg);
	if (ret < 0)
		pr_err("%s: regulator_enable failed\n", __func__);
	else
		nfc_dev->is_vreg_enabled = true;
	return ret;
}

/**
 * nfc_ldo_config()
 * @dev: device instance to read DT entry
 * @nfc_dev: NFC device containing regulator handle
 *
 * Configure LDO if entry is present in DT file otherwise
 * return with success as it's optional
 *
 * Return: 0 on success and -ve on failure
 */
int nfc_ldo_config(struct device *dev, struct nfc_dev *nfc_dev)
{
	int ret;

	if (of_get_property(dev->of_node, NFC_LDO_SUPPLY_NAME, NULL)) {
		// Get the regulator handle
		nfc_dev->reg = regulator_get(dev, NFC_LDO_SUPPLY_DT_NAME);
		if (IS_ERR(nfc_dev->reg)) {
			ret = PTR_ERR(nfc_dev->reg);
			nfc_dev->reg = NULL;
			pr_err("%s: regulator_get failed, ret = %d\n",
				__func__, ret);
			return ret;
		}
	} else {
		nfc_dev->reg = NULL;
		pr_err("%s: regulator entry not present\n", __func__);
		// return success as it's optional to configure LDO
		return 0;
	}

	// LDO config supported by platform DT
	ret = nfc_ldo_vote(nfc_dev);
	if (ret < 0) {
		pr_err("%s: LDO voting failed, ret = %d\n", __func__, ret);
		regulator_put(nfc_dev->reg);
	}
	return ret;
}

/**
 * nfc_ldo_unvote()
 * @nfc_dev: NFC device containing regulator handle
 *
 * set voltage and load to zero and disable regulator
 *
 * Return: 0 on success and -ve on failure
 */
int nfc_ldo_unvote(struct nfc_dev *nfc_dev)
{
	int ret;

	if (!nfc_dev->is_vreg_enabled) {
		pr_err("%s: regulator already disabled\n", __func__);
		return -EINVAL;
	}

	ret = regulator_disable(nfc_dev->reg);
	if (ret < 0) {
		pr_err("%s: regulator_disable failed\n", __func__);
		return ret;
	}
	nfc_dev->is_vreg_enabled = false;

	ret =  regulator_set_voltage(nfc_dev->reg, 0, NFC_VDDIO_MAX);
	if (ret < 0) {
		pr_err("%s: set voltage failed\n", __func__);
		return ret;
	}

	ret = regulator_set_load(nfc_dev->reg, 0);
	if (ret < 0)
		pr_err("%s: set load failed\n", __func__);
	return ret;
}

void gpio_set_ven(struct nfc_dev *nfc_dev, int value)
{
	if (gpio_get_value(nfc_dev->gpio.ven) != value) {
		gpio_set_value(nfc_dev->gpio.ven, value);
		// hardware dependent delay
		usleep_range(10000, 10100);
	}
}

int configure_gpio(unsigned int gpio, int flag)
{
	int ret;

	pr_debug("%s: nfc gpio [%d] flag [%01x]\n", __func__, gpio, flag);

	if (gpio_is_valid(gpio)) {
		ret = gpio_request(gpio, "nfc_gpio");
		if (ret) {
			pr_err("%s: unable to request nfc gpio [%d]\n",
			       __func__, gpio);
			return ret;
		}
		// set direction and value for output pin
		if (flag & GPIO_OUTPUT)
			ret = gpio_direction_output(gpio, (GPIO_HIGH & flag));
		else
			ret = gpio_direction_input(gpio);

		if (ret) {
			pr_err
			    ("%s: unable to set direction for nfc gpio [%d]\n",
			     __func__, gpio);
			gpio_free(gpio);
			return ret;
		}
		// Consider value as control for input IRQ pin
		if (flag & GPIO_IRQ) {
			ret = gpio_to_irq(gpio);
			if (ret < 0) {
				pr_err("%s: unable to set irq for nfc gpio [%d]\n",
				     __func__, gpio);
				gpio_free(gpio);
				return ret;
			}
			pr_debug
			    ("%s: gpio_to_irq successful [%d]\n",
			     __func__, gpio);
			return ret;
		}
	} else {
		pr_err("%s: invalid gpio\n", __func__);
		ret = -EINVAL;
	}
	return ret;
}

void nfc_misc_remove(struct nfc_dev *nfc_dev, int count)
{
	pr_debug("%s: entry\n", __func__);

	kfree(nfc_dev->kbuf);
	device_destroy(nfc_dev->nfc_class, nfc_dev->devno);
	cdev_del(&nfc_dev->c_dev);
	class_destroy(nfc_dev->nfc_class);
	unregister_chrdev_region(nfc_dev->devno, count);
	if (nfc_dev->ipcl)
		ipc_log_context_destroy(nfc_dev->ipcl);
}

int nfc_misc_probe(struct nfc_dev *nfc_dev,
		      const struct file_operations *nfc_fops, int count,
		      char *devname, char *classname)
{
	int ret = 0;

	ret = alloc_chrdev_region(&nfc_dev->devno, 0, count, devname);
	if (ret < 0) {
		pr_err("%s: failed to alloc chrdev region ret %d\n",
			__func__, ret);
		return ret;
	}
	nfc_dev->nfc_class = class_create(THIS_MODULE, classname);
	if (IS_ERR(nfc_dev->nfc_class)) {
		ret = PTR_ERR(nfc_dev->nfc_class);
		pr_err("%s: failed to register device class ret %d\n",
			__func__, ret);
		unregister_chrdev_region(nfc_dev->devno, count);
		return ret;
	}
	cdev_init(&nfc_dev->c_dev, nfc_fops);
	ret = cdev_add(&nfc_dev->c_dev, nfc_dev->devno, count);
	if (ret < 0) {
		pr_err("%s: failed to add cdev ret %d\n", __func__, ret);
		class_destroy(nfc_dev->nfc_class);
		unregister_chrdev_region(nfc_dev->devno, count);
		return ret;
	}
	nfc_dev->nfc_device = device_create(nfc_dev->nfc_class, NULL,
					    nfc_dev->devno, nfc_dev, devname);
	if (IS_ERR(nfc_dev->nfc_device)) {
		ret = PTR_ERR(nfc_dev->nfc_device);
		pr_err("%s: failed to create the device ret %d\n",
			__func__, ret);
		cdev_del(&nfc_dev->c_dev);
		class_destroy(nfc_dev->nfc_class);
		unregister_chrdev_region(nfc_dev->devno, count);
		return ret;
	}

	nfc_dev->ipcl = ipc_log_context_create(NUM_OF_IPC_LOG_PAGES,
						dev_name(nfc_dev->nfc_device), 0);

	nfc_dev->kbuflen = MAX_BUFFER_SIZE;
	nfc_dev->kbuf = kzalloc(MAX_BUFFER_SIZE, GFP_KERNEL | GFP_DMA);
	if (!nfc_dev->kbuf) {
		nfc_misc_remove(nfc_dev, count);
		return -ENOMEM;
	}

	nfc_dev->cold_reset.rsp_pending = false;
	nfc_dev->cold_reset.is_nfc_enabled = false;
	init_waitqueue_head(&nfc_dev->cold_reset.read_wq);

	return 0;
}


static int send_cold_reset_cmd(struct nfc_dev *nfc_dev)
{
	int ret = 0;
	char *cold_reset_cmd = NULL;

	cold_reset_cmd = kzalloc(COLD_RESET_CMD_LEN, GFP_DMA | GFP_KERNEL);
	if (!cold_reset_cmd)
		return -ENOMEM;

	if (gpio_get_value(nfc_dev->gpio.dwl_req)) {
		pr_err("FW download in-progress\n");
		ret = -EBUSY;
		goto error;
	}
	if (!gpio_get_value(nfc_dev->gpio.ven)) {
		pr_err("VEN LOW - NFCC powered off\n");
		ret = -ENODEV;
		goto error;
	}

	cold_reset_cmd[0] = COLD_RESET_CMD_GID;
	cold_reset_cmd[1] = COLD_RESET_OID;
	cold_reset_cmd[2] = COLD_RESET_CMD_PAYLOAD_LEN;

	ret = nfc_dev->nfc_write(nfc_dev, cold_reset_cmd,
					COLD_RESET_CMD_LEN, MAX_RETRY_COUNT);
	if (ret <= 0)
		pr_err("%s: write failed after max retry, ret %d\n",
			__func__, ret);

error:
	kfree(cold_reset_cmd);
	return ret;
}

void read_cold_reset_rsp(struct nfc_dev *nfc_dev, char *header)
{
	int ret = -1;
	char *cold_reset_rsp = NULL;
	struct cold_reset *cold_reset = &nfc_dev->cold_reset;

	cold_reset_rsp = kzalloc(COLD_RESET_RSP_LEN, GFP_DMA | GFP_KERNEL);
	if (!cold_reset_rsp)
		return;

	/*
	 * read header also if NFC is disabled
	 * for enable case, will be taken care by nfc read thread
	 */
	if ((!cold_reset->is_nfc_enabled) &&
		(nfc_dev->interface == PLATFORM_IF_I2C)) {

		ret = nfc_dev->nfc_read(nfc_dev, cold_reset_rsp,
						NCI_HDR_LEN);
		if (ret <= 0) {
			pr_err("%s: failure to read cold reset rsp header\n",
			       __func__);
			goto error;
		}
	} else if (header) {

		/* For I3C driver, header is read by the worker thread */
		memcpy(cold_reset_rsp, header, NCI_HDR_LEN);

	} else {
		pr_err("%s: - invalid or NULL header\n", __func__);
		goto error;
	}

	if ((cold_reset_rsp[0] != COLD_RESET_RSP_GID)
	    || (cold_reset_rsp[1] != COLD_RESET_OID)) {
		pr_err("%s: - invalid response GID or OID for cold_reset\n",
		       __func__);
		ret = -EINVAL;
		goto error;
	}
	if ((NCI_HDR_LEN + cold_reset_rsp[2]) > COLD_RESET_RSP_LEN) {
		pr_err("%s: - invalid response for cold_reset\n", __func__);
		ret = -EINVAL;
		goto error;
	}

	if (nfc_dev->interface == PLATFORM_IF_I2C)
		ret = nfc_dev->nfc_read(nfc_dev,
			     &cold_reset_rsp[NCI_PAYLOAD_IDX],
			     cold_reset_rsp[2]);
	else
		ret = nfc_dev->i3c_dev.nfc_read_direct(nfc_dev,
			     &cold_reset_rsp[NCI_PAYLOAD_IDX],
			     cold_reset_rsp[2]);

	if (ret <= 0) {
		pr_err("%s: failure to read cold reset rsp payload\n",
			__func__);
		goto error;
	}
	cold_reset->status = cold_reset_rsp[NCI_PAYLOAD_IDX];

error:
	kfree(cold_reset_rsp);
}

/*
 * Power management of the eSE
 * eSE and NFCC both are powered using VEN gpio,
 * VEN HIGH - eSE and NFCC both are powered on
 * VEN LOW - eSE and NFCC both are power down
 */
int nfc_ese_pwr(struct nfc_dev *nfc_dev, unsigned long arg)
{
	int ret = 0;

	if (arg == ESE_POWER_ON) {
		/*
		 * Let's store the NFC VEN pin state
		 * will check stored value in case of eSE power off request,
		 * to find out if NFC MW also sent request to set VEN HIGH
		 * VEN state will remain HIGH if NFC is enabled otherwise
		 * it will be set as LOW
		 */
		nfc_dev->nfc_ven_enabled = gpio_get_value(nfc_dev->gpio.ven);
		if (!nfc_dev->nfc_ven_enabled) {
			pr_debug("eSE HAL service setting ven HIGH\n");
			gpio_set_ven(nfc_dev, 1);
		} else {
			pr_debug("ven already HIGH\n");
		}
		nfc_dev->is_ese_session_active = true;
	} else if (arg == ESE_POWER_OFF) {
		if (!nfc_dev->nfc_ven_enabled) {
			pr_debug("NFC not enabled, disabling ven\n");
			gpio_set_ven(nfc_dev, 0);
		} else {
			pr_debug("keep ven high as NFC is enabled\n");
		}
		nfc_dev->is_ese_session_active = false;
	} else if (arg == ESE_COLD_RESET) {

		// set default value for status as failure
		nfc_dev->cold_reset.status = -EIO;

		ret = send_cold_reset_cmd(nfc_dev);
		if (ret <= 0) {
			pr_err("failed to send cold reset command\n");
			return nfc_dev->cold_reset.status;
		}

		nfc_dev->cold_reset.rsp_pending = true;

		// check if NFC is enabled
		if (nfc_dev->cold_reset.is_nfc_enabled) {

			/*
			 * nfc_read thread will initiate cold reset response
			 * and it will signal for data available
			 */
			wait_event_interruptible(nfc_dev->cold_reset.read_wq,
				!nfc_dev->cold_reset.rsp_pending);

		} else {

			// Read data as NFC thread is not active

			nfc_dev->nfc_enable_intr(nfc_dev);

			if (nfc_dev->interface == PLATFORM_IF_I2C) {
				ret = wait_event_interruptible_timeout(
					nfc_dev->read_wq,
					!nfc_dev->i2c_dev.irq_enabled,
					msecs_to_jiffies(MAX_IRQ_WAIT_TIME));
				if (ret <= 0) {
					nfc_dev->nfc_disable_intr(nfc_dev);
					nfc_dev->cold_reset.rsp_pending = false;
					return nfc_dev->cold_reset.status;
				}
				read_cold_reset_rsp(nfc_dev, NULL);
				nfc_dev->cold_reset.rsp_pending = false;
			} else {
				wait_event_interruptible(
					nfc_dev->cold_reset.read_wq,
					!nfc_dev->cold_reset.rsp_pending);
				nfc_dev->nfc_disable_intr(nfc_dev);
			}
		}

		ret = nfc_dev->cold_reset.status;

	} else if (arg == ESE_POWER_STATE) {
		// eSE power state
		ret = gpio_get_value(nfc_dev->gpio.ven);
	} else {
		pr_err("%s bad arg %lu\n", __func__, arg);
		ret = -ENOIOCTLCMD;
	}
	return ret;
}

/*
 * nfc_ioctl_power_states() - power control
 * @nfc_dev:    nfc device data structure
 * @arg:    mode that we want to move to
 *
 * Device power control. Depending on the arg value, device moves to
 * different states, refer nfcc_ioctl_request in nfc_common.h for args
 *
 * Return: -ENOIOCTLCMD if arg is not supported, 0 in any other case
 */
static int nfc_ioctl_power_states(struct nfc_dev *nfc_dev, unsigned long arg)
{
	int ret = 0;

	if (arg == NFC_POWER_OFF) {
		/*
		 * We are attempting a hardware reset so let us disable
		 * interrupts to avoid spurious notifications to upper
		 * layers.
		 */
		nfc_dev->nfc_disable_intr(nfc_dev);
		pr_debug("gpio firm disable\n");
		if (gpio_is_valid(nfc_dev->gpio.dwl_req)) {
			gpio_set_value(nfc_dev->gpio.dwl_req, 0);
			usleep_range(10000, 10100);
		}

		pr_debug("Set ven to low\n");
		gpio_set_ven(nfc_dev, 0);

		nfc_dev->nfc_ven_enabled = false;

	} else if (arg == NFC_POWER_ON) {
		nfc_dev->nfc_enable_intr(nfc_dev);
		pr_debug("gpio_set_value enable: %s:\n", __func__);
		if (gpio_is_valid(nfc_dev->gpio.dwl_req)) {
			gpio_set_value(nfc_dev->gpio.dwl_req, 0);
			usleep_range(10000, 10100);
		}
		gpio_set_ven(nfc_dev, 1);
		nfc_dev->nfc_ven_enabled = true;

		if (nfc_dev->interface == PLATFORM_IF_I3C)
			nfc_dev->i3c_dev.read_hdr = NCI_HDR_LEN;

	} else if (arg == NFC_FW_DWL_VEN_TOGGLE) {
		/*
		 * We are switching to download Mode, toggle the enable pin
		 * in order to set the NFCC in the new mode
		 */

		gpio_set_ven(nfc_dev, 1);

		if (gpio_is_valid(nfc_dev->gpio.dwl_req)) {
			gpio_set_value(nfc_dev->gpio.dwl_req, 1);
			usleep_range(10000, 10100);
		}
		if (nfc_dev->interface == PLATFORM_IF_I2C) {
			gpio_set_ven(nfc_dev, 0);
			gpio_set_ven(nfc_dev, 1);
		}

	} else if (arg == NFC_FW_DWL_HIGH) {
		/*
		 * Setting firmware download gpio to HIGH
		 * before FW download start
		 */
		pr_debug("set fw gpio high\n");
		if (gpio_is_valid(nfc_dev->gpio.dwl_req)) {
			gpio_set_value(nfc_dev->gpio.dwl_req, 1);
			usleep_range(10000, 10100);
		} else
			pr_debug("gpio.dwl_req is invalid\n");

	} else if (arg == NFC_VEN_FORCED_HARD_RESET
		   && nfc_dev->interface == PLATFORM_IF_I2C) {
		/*
		 * TODO: Enable Ven reset for I3C, after hot join integration
		 */

		gpio_set_value(nfc_dev->gpio.ven, 0);
		usleep_range(10000, 10100);
		gpio_set_value(nfc_dev->gpio.ven, 1);
		usleep_range(10000, 10100);
		pr_info("%s VEN forced reset done\n", __func__);

	} else if (arg == NFC_FW_DWL_LOW) {
		/*
		 * Setting firmware download gpio to LOW
		 * FW download finished
		 */
		pr_debug("set fw gpio LOW\n");
		gpio_set_value(nfc_dev->gpio.dwl_req, 0);
		usleep_range(10000, 10100);

		if (nfc_dev->interface == PLATFORM_IF_I3C)
			nfc_dev->i3c_dev.read_hdr = NCI_HDR_LEN;

	} else if (arg == NFC_FW_HDR_LEN) {
		if (nfc_dev->interface == PLATFORM_IF_I3C)
			nfc_dev->i3c_dev.read_hdr = FW_HDR_LEN;
	} else if (arg == NFC_ENABLE) {
		/*
		 * Setting flag true when NFC is enabled
		 */
		nfc_dev->cold_reset.is_nfc_enabled = true;
	} else if (arg == NFC_DISABLE) {
		/*
		 * Setting flag true when NFC is disabled
		 */
		nfc_dev->cold_reset.is_nfc_enabled = false;
	}  else {
		pr_err("%s bad arg %lu\n", __func__, arg);
		ret = -ENOIOCTLCMD;
	}
	return ret;
}

/*
 * Inside nfc_ioctl_nfcc_info
 *
 * @brief   nfc_ioctl_nfcc_info
 *
 * Check the NFC Chipset and firmware version details
 */
unsigned int nfc_ioctl_nfcc_info(struct file *filp, unsigned long arg)
{
	unsigned int r = 0;
	struct nfc_dev *nfc_dev = filp->private_data;

	r = nfc_dev->nqx_info.i;
	pr_debug("nfc : %s r = %d\n", __func__, r);

	return r;
}

/** @brief   IOCTL function  to be used to set or get data from upper layer.
 *
 *  @param   pfile  fil node for opened device.
 *  @cmd     IOCTL type from upper layer.
 *  @arg     IOCTL arg from upper layer.
 *
 *  @return 0 on success, error code for failures.
 */
long nfc_dev_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct nfc_dev *nfc_dev = pfile->private_data;

	if (!nfc_dev)
		return -ENODEV;

	pr_debug("%s cmd = %x arg = %zx\n", __func__, cmd, arg);

	switch (cmd) {
	case NFC_SET_PWR:
		ret = nfc_ioctl_power_states(nfc_dev, arg);
		break;
	case ESE_SET_PWR:
		ret = nfc_ese_pwr(nfc_dev, arg);
		break;
	case ESE_GET_PWR:
		ret = nfc_ese_pwr(nfc_dev, ESE_POWER_STATE);
		break;
	case NFCC_GET_INFO:
		ret = nfc_ioctl_nfcc_info(pfile, arg);
		break;
	case NFC_GET_PLATFORM_TYPE:
		ret = nfc_dev->interface;
		break;
	default:
		pr_err("%s bad cmd %lu\n", __func__, arg);
		ret = -ENOIOCTLCMD;
	}
	return ret;
}

int nfc_dev_open(struct inode *inode, struct file *filp)
{
	struct nfc_dev *nfc_dev = container_of(inode->i_cdev,
					struct nfc_dev, c_dev);

	if (!nfc_dev)
		return -ENODEV;

	pr_debug("%s: %d, %d\n", __func__, imajor(inode), iminor(inode));

	mutex_lock(&nfc_dev->dev_ref_mutex);

	filp->private_data = nfc_dev;

	if (nfc_dev->dev_ref_count == 0) {
		if (gpio_is_valid(nfc_dev->gpio.dwl_req)) {
			gpio_set_value(nfc_dev->gpio.dwl_req, 0);
			usleep_range(10000, 10100);
		}
		nfc_dev->nfc_enable_intr(nfc_dev);
	}
	nfc_dev->dev_ref_count = nfc_dev->dev_ref_count + 1;

	mutex_unlock(&nfc_dev->dev_ref_mutex);

	return 0;
}

int nfc_dev_close(struct inode *inode, struct file *filp)
{
	struct nfc_dev *nfc_dev = container_of(inode->i_cdev,
					struct nfc_dev, c_dev);

	if (!nfc_dev)
		return -ENODEV;

	pr_debug("%s: %d, %d\n", __func__, imajor(inode), iminor(inode));

	mutex_lock(&nfc_dev->dev_ref_mutex);

	if (nfc_dev->dev_ref_count == 1) {

		nfc_dev->nfc_disable_intr(nfc_dev);

		if (gpio_is_valid(nfc_dev->gpio.dwl_req)) {
			gpio_set_value(nfc_dev->gpio.dwl_req, 0);
			usleep_range(10000, 10100);
		}
	}

	if (nfc_dev->dev_ref_count > 0)
		nfc_dev->dev_ref_count = nfc_dev->dev_ref_count - 1;

	filp->private_data = NULL;

	mutex_unlock(&nfc_dev->dev_ref_mutex);

	return 0;
}

int is_data_available_for_read(struct nfc_dev *nfc_dev)
{
	int ret;

	nfc_dev->nfc_enable_intr(nfc_dev);

	ret = wait_event_interruptible_timeout(nfc_dev->read_wq,
			!nfc_dev->i2c_dev.irq_enabled,
			msecs_to_jiffies(MAX_IRQ_WAIT_TIME));
	return ret;
}

/* Check for availability of NFC controller hardware */
int nfcc_hw_check(struct nfc_dev *nfc_dev)
{
	int ret = 0;
	unsigned char reset_ntf_len = 0;
	char *nci_reset_cmd = NULL;
	char *nci_reset_rsp = NULL;
	char *nci_reset_ntf = NULL;
	char *nci_get_version_cmd = NULL;
	char *nci_get_version_rsp = NULL;

	NFCLOG_IPC(nfc_dev, false, "%s", __func__);

	nci_reset_cmd = kzalloc(NCI_RESET_CMD_LEN + 1, GFP_DMA | GFP_KERNEL);
	if (!nci_reset_cmd)
		return -ENOMEM;

	nci_reset_rsp = kzalloc(NCI_RESET_RSP_LEN + 1,  GFP_DMA | GFP_KERNEL);
	if (!nci_reset_rsp) {
		ret = -ENOMEM;
		goto done;
	}

	nci_reset_ntf = kzalloc(NCI_RESET_NTF_LEN + 1,  GFP_DMA | GFP_KERNEL);
	if (!nci_reset_ntf) {
		ret = -ENOMEM;
		goto done;
	}

	nci_get_version_cmd = kzalloc(NCI_GET_VERSION_CMD_LEN + 1,
					GFP_DMA | GFP_KERNEL);
	if (!nci_get_version_cmd) {
		ret = -ENOMEM;
		goto done;
	}

	nci_get_version_rsp = kzalloc(NCI_GET_VERSION_RSP_LEN + 1,
					GFP_DMA | GFP_KERNEL);
	if (!nci_get_version_rsp) {
		ret = -ENOMEM;
		goto done;
	}

	if (nfc_dev->interface == PLATFORM_IF_I3C)
		nfc_dev->nfc_enable_intr(nfc_dev);
	else {
		/* making sure that the NFCC starts in a clean state. */
		gpio_set_ven(nfc_dev, 1);/* HPD : Enable*/
		gpio_set_ven(nfc_dev, 0);/* ULPM: Disable */
		gpio_set_ven(nfc_dev, 1);/* HPD : Enable*/
	}

	nci_reset_cmd[0] = 0x20;
	nci_reset_cmd[1] = 0x00;
	nci_reset_cmd[2] = 0x01;
	nci_reset_cmd[3] = 0x00;

	/* send NCI CORE RESET CMD with Keep Config parameters */
	ret = nfc_dev->nfc_write(nfc_dev, nci_reset_cmd, NCI_RESET_CMD_LEN,
				MAX_RETRY_COUNT);
	if (ret <= 0) {
		pr_err("%s: - nfc core reset error\n", __func__);

		if (gpio_is_valid(nfc_dev->gpio.dwl_req)) {
			gpio_set_value(nfc_dev->gpio.dwl_req, 1);
			usleep_range(10000, 10100);
		}

		if (nfc_dev->interface == PLATFORM_IF_I2C) {
			gpio_set_ven(nfc_dev, 0);
			gpio_set_ven(nfc_dev, 1);
		}

		nci_get_version_cmd[0] = 0x00;
		nci_get_version_cmd[1] = 0x04;
		nci_get_version_cmd[2] = 0xF1;
		nci_get_version_cmd[3] = 0x00;
		nci_get_version_cmd[4] = 0x00;
		nci_get_version_cmd[5] = 0x00;
		nci_get_version_cmd[6] = 0x6E;
		nci_get_version_cmd[7] = 0xEF;

		ret = nfc_dev->nfc_write(nfc_dev, nci_get_version_cmd,
				NCI_GET_VERSION_CMD_LEN, MAX_RETRY_COUNT);
		if (ret <= 0) {
			pr_err("%s: - nfc get version cmd error ret %d\n",
					__func__, ret);
			goto err_nfcc_hw_check;
		}

		if (nfc_dev->interface == PLATFORM_IF_I2C) {
			ret = is_data_available_for_read(nfc_dev);
			if (ret <= 0) {
				nfc_dev->nfc_disable_intr(nfc_dev);
				pr_err("%s: - error waiting for get version rsp ret %d\n",
					__func__, ret);
				goto err_nfcc_hw_check;
			}
		}

		ret = nfc_dev->nfc_read(nfc_dev, nci_get_version_rsp,
					NCI_GET_VERSION_RSP_LEN);
		if (ret <= 0) {
			pr_err("%s: - nfc get version rsp error ret %d\n",
				__func__, ret);
			goto err_nfcc_hw_check;
		} else {
			nfc_dev->nqx_info.info.chip_type =
				nci_get_version_rsp[3];
			nfc_dev->nqx_info.info.rom_version =
				nci_get_version_rsp[4];
			nfc_dev->nqx_info.info.fw_minor =
				nci_get_version_rsp[6];
			nfc_dev->nqx_info.info.fw_major =
				nci_get_version_rsp[7];
		}

		gpio_set_value(nfc_dev->gpio.dwl_req, 0);

		goto err_nfcc_reset_failed;
	}

	if (nfc_dev->interface == PLATFORM_IF_I2C) {
		ret = is_data_available_for_read(nfc_dev);
		if (ret <= 0) {
			nfc_dev->nfc_disable_intr(nfc_dev);
			pr_err("%s: - error waiting for core reset rsp ret %d\n",
					__func__, ret);

			goto err_nfcc_hw_check;
		}
	}

	/* Read Response of RESET command */
	ret = nfc_dev->nfc_read(nfc_dev, nci_reset_rsp, NCI_RESET_RSP_LEN);
	if (ret <= 0) {
		pr_err("%s: - nfc rst rsp read err %d\n", __func__,
					ret);
		goto err_nfcc_hw_check;
	}

	if (nfc_dev->interface == PLATFORM_IF_I2C) {
		ret = is_data_available_for_read(nfc_dev);
		if (ret <= 0) {
			pr_err("%s: - error waiting for core reset ntf ret %d\n",
					__func__, ret);
			nfc_dev->nfc_disable_intr(nfc_dev);
			goto err_nfcc_hw_check;
		}
	}

	/* Read Notification of RESET command */
	ret = nfc_dev->nfc_read(nfc_dev, nci_reset_ntf, NCI_RESET_NTF_LEN);
	if (ret <= 0) {
		pr_err("%s: nfc nfc read error %d\n", __func__, ret);
		goto err_nfcc_hw_check;
	}

	reset_ntf_len = NCI_HDR_LEN + nci_reset_ntf[NCI_PAYLOAD_LEN_IDX] - 1;
	if (reset_ntf_len > NCI_HDR_LEN) {
		nfc_dev->nqx_info.info.chip_type =
			nci_reset_ntf[reset_ntf_len - NFC_CHIP_TYPE_OFF];
		nfc_dev->nqx_info.info.rom_version =
			nci_reset_ntf[reset_ntf_len - NFC_ROM_VERSION_OFF];
		nfc_dev->nqx_info.info.fw_major =
			nci_reset_ntf[reset_ntf_len - NFC_FW_MAJOR_OFF];
		nfc_dev->nqx_info.info.fw_minor =
				nci_reset_ntf[reset_ntf_len];
	}
	pr_debug("%s: - NFC reset rsp : NfcNciRx %x %x %x\n",
		__func__, nci_reset_rsp[0],
		nci_reset_rsp[1], nci_reset_rsp[2]);

err_nfcc_reset_failed:
	pr_info("NFC chip_type = %x\n",
		nfc_dev->nqx_info.info.chip_type);
	pr_info("NFC fw version = %x.%x.%x\n",
		nfc_dev->nqx_info.info.rom_version,
		nfc_dev->nqx_info.info.fw_major,
		nfc_dev->nqx_info.info.fw_minor);

	switch (nfc_dev->nqx_info.info.chip_type) {
	case NFCC_SN100_A:
	case NFCC_SN100_B:
		pr_debug("%s: ## NFCC == SN100x ##\n", __func__);
		break;
	default:
		pr_err("%s: - NFCC HW not Supported\n", __func__);
		break;
	}

	ret = 0;
	nfc_dev->nfc_ven_enabled = true;

	goto disable_i3c_intr;

err_nfcc_hw_check:
	if (nfc_dev->interface == PLATFORM_IF_I2C)
		gpio_set_ven(nfc_dev, 0);

	gpio_set_value(nfc_dev->gpio.dwl_req, 0);

	ret = -ENXIO;
	pr_debug("%s: - NFCC HW not available\n", __func__);

disable_i3c_intr:
	if (nfc_dev->interface == PLATFORM_IF_I3C)
		nfc_dev->nfc_disable_intr(nfc_dev);
done:
	kfree(nci_reset_rsp);
	kfree(nci_reset_ntf);
	kfree(nci_get_version_cmd);
	kfree(nci_get_version_rsp);
	kfree(nci_reset_cmd);

	return ret;
}
