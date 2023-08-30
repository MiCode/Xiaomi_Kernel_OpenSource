// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2019-2021 NXP
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

#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include "nfc_common.h"

int nfc_parse_dt(struct device *dev, struct platform_configs *nfc_configs,
		 uint8_t interface)
{
	int ret;
	struct device_node *np = dev->of_node;
	struct platform_gpio *nfc_gpio = &nfc_configs->gpio;
	struct platform_ldo *ldo = &nfc_configs->ldo;

	if (!np) {
		pr_err("nfc of_node NULL\n");
		return -EINVAL;
	}

	nfc_gpio->irq = -EINVAL;
	nfc_gpio->dwl_req = -EINVAL;
	nfc_gpio->ven = -EINVAL;
	nfc_gpio->clkreq = -EINVAL;

	/* required for i2c based chips only */
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

	/* not returning failure for dwl gpio as it is optional for sn220 */
	if ((!gpio_is_valid(nfc_gpio->dwl_req)))
		pr_warn("nfc dwl_req gpio invalid %d\n", nfc_gpio->dwl_req);

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
			nfc_dev->configs.ldo.vdd_levels[0],
			nfc_dev->configs.ldo.vdd_levels[1]);
	if (ret < 0) {
		pr_err("%s: set voltage failed\n", __func__);
		return ret;
	}

	/* pass expected current from NFC in uA */
	ret = regulator_set_load(nfc_dev->reg, nfc_dev->configs.ldo.max_current);
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

void set_valid_gpio(int gpio, int value)
{
	if (gpio_is_valid(gpio)) {
		pr_debug("%s gpio %d value %d\n", __func__, gpio, value);
		gpio_set_value(gpio, value);
		/* hardware dependent delay */
		usleep_range(NFC_GPIO_SET_WAIT_TIME_USEC,
			     NFC_GPIO_SET_WAIT_TIME_USEC + 100);
	}
}

int get_valid_gpio(int gpio)
{
	int value = -EINVAL;

	if (gpio_is_valid(gpio)) {
		value = gpio_get_value(gpio);
		pr_debug("%s gpio %d value %d\n", __func__, gpio, value);
	}
	return value;
}

void gpio_set_ven(struct nfc_dev *nfc_dev, int value)
{
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	if (gpio_get_value(nfc_gpio->ven) != value) {
		pr_debug("%s: value %d\n", __func__, value);

		gpio_set_value(nfc_gpio->ven, value);
		/* hardware dependent delay */
		if(value == 0)
		{
			usleep_range(2*NFC_GPIO_SET_WAIT_TIME_USEC,
			     2*NFC_GPIO_SET_WAIT_TIME_USEC + 100);
		} else {
			usleep_range(NFC_GPIO_SET_WAIT_TIME_USEC,
			     NFC_GPIO_SET_WAIT_TIME_USEC + 100);
		}
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
		/* set direction and value for output pin */
		if (flag & GPIO_OUTPUT) {
			ret = gpio_direction_output(gpio, (GPIO_HIGH & flag));
			pr_debug("nfc o/p gpio %d level %d\n", gpio, gpio_get_value(gpio));
		} else {
			ret = gpio_direction_input(gpio);
			pr_debug("nfc i/p gpio %d\n", gpio);
		}

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

void nfc_misc_unregister(struct nfc_dev *nfc_dev, int count)
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

int nfc_misc_register(struct nfc_dev *nfc_dev,
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
		nfc_misc_unregister(nfc_dev, count);
		return -ENOMEM;
	}

	nfc_dev->cold_reset.rsp_pending = false;
	nfc_dev->cold_reset.is_nfc_enabled = false;
	nfc_dev->cold_reset.is_crp_en = false;
	nfc_dev->cold_reset.last_src_ese_prot = ESE_COLD_RESET_ORIGIN_NONE;

	init_waitqueue_head(&nfc_dev->cold_reset.read_wq);

	return 0;
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
	pr_info("%s : enter, arg=%d\n", __func__, arg);
	if (arg == ESE_POWER_ON) {
		/*
		 * Let's store the NFC VEN pin state
		 * will check stored value in case of eSE power off request,
		 * to find out if NFC MW also sent request to set VEN HIGH
		 * VEN state will remain HIGH if NFC is enabled otherwise
		 * it will be set as LOW
		 */
		nfc_dev->nfc_ven_enabled = gpio_get_value(nfc_dev->configs.gpio.ven);
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
	} else if (arg == ESE_POWER_STATE) {
		/* get VEN gpio state for eSE, as eSE also enabled through same GPIO */
		ret = gpio_get_value(nfc_dev->configs.gpio.ven);
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
 * different states, refer common.h for args
 *
 * Return: -ENOIOCTLCMD if arg is not supported, 0 if Success(or no issue)
 * and error ret code otherwise
 */
static int nfc_ioctl_power_states(struct nfc_dev *nfc_dev, unsigned long arg)
{
	int ret = 0;
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;
	pr_info("%s : enter, arg=%d \n", __func__, arg);
	if (arg == NFC_POWER_OFF) {
		/*
		 * We are attempting a hardware reset so let us disable
		 * interrupts to avoid spurious notifications to upper
		 * layers.
		 */
		nfc_dev->nfc_disable_intr(nfc_dev);
		set_valid_gpio(nfc_gpio->dwl_req, 0);
		gpio_set_ven(nfc_dev, 0);
		nfc_dev->nfc_ven_enabled = false;

	} else if (arg == NFC_POWER_ON) {
		nfc_dev->nfc_enable_intr(nfc_dev);
		set_valid_gpio(nfc_gpio->dwl_req, 0);

		gpio_set_ven(nfc_dev, 1);
		nfc_dev->nfc_ven_enabled = true;

	} else if (arg == NFC_FW_DWL_VEN_TOGGLE) {
		/*
		 * We are switching to download Mode, toggle the enable pin
		 * in order to set the NFCC in the new mode
		 */
		nfc_dev->nfc_disable_intr(nfc_dev);
		set_valid_gpio(nfc_gpio->dwl_req, 1);
		nfc_dev->nfc_state = NFC_STATE_FW_DWL;
		gpio_set_ven(nfc_dev, 0);
		gpio_set_ven(nfc_dev, 1);
		nfc_dev->nfc_enable_intr(nfc_dev);
	} else if (arg == NFC_FW_DWL_HIGH) {
		/*
		 * Setting firmware download gpio to HIGH
		 * before FW download start
		 */
		pr_info("set fw gpio high\n");
		set_valid_gpio(nfc_gpio->dwl_req, 1);
		nfc_dev->nfc_state = NFC_STATE_FW_DWL;

	} else if (arg == NFC_VEN_FORCED_HARD_RESET) {
		nfc_dev->nfc_disable_intr(nfc_dev);
		gpio_set_ven(nfc_dev, 0);
		gpio_set_ven(nfc_dev, 1);
		nfc_dev->nfc_enable_intr(nfc_dev);
		pr_info("%s VEN forced reset done\n", __func__);

	} else if (arg == NFC_FW_DWL_LOW) {
		/*
		 * Setting firmware download gpio to LOW
		 * FW download finished
		 */
		pr_info("set fw gpio LOW\n");
		set_valid_gpio(nfc_gpio->dwl_req, 0);
		nfc_dev->nfc_state = NFC_STATE_NCI;

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
	pr_debug("nfc : %s r = 0x%x\n", __func__, r);

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

	pr_info("%s cmd = %x arg = %zx\n", __func__, cmd, arg);

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
	case ESE_COLD_RESET:
		pr_debug("nfc ese cold reset ioctl\n");
		ret = ese_cold_reset_ioctl(nfc_dev, arg);
		break;
	case NFC_GET_IRQ_STATE:
		ret = gpio_get_value(nfc_dev->configs.gpio.irq);
		break;
	default:
		pr_err("%s Unsupported ioctl cmd 0x%x, arg %lu\n",
						__func__, cmd, arg);
		ret = -ENOIOCTLCMD;
	}
	return ret;
}

int nfc_dev_open(struct inode *inode, struct file *filp)
{
	struct nfc_dev *nfc_dev = NULL;
	nfc_dev = container_of(inode->i_cdev, struct nfc_dev, c_dev);

	if (!nfc_dev)
		return -ENODEV;

	pr_debug("%s: %d, %d\n", __func__, imajor(inode), iminor(inode));

	/* Set flag to block freezer fake signal if not set already.
	 * Without this Signal being set, Driver is trying to do a read
	 * which is causing the delay in moving to Hibernate Mode.
	 */
	if (!(current->flags & PF_NOFREEZE)) {
		current->flags |= PF_NOFREEZE;
		pr_debug("%s: current->flags 0x%x.\n", __func__, current->flags);
	}

	mutex_lock(&nfc_dev->dev_ref_mutex);

	filp->private_data = nfc_dev;

	if (nfc_dev->dev_ref_count == 0) {
		set_valid_gpio(nfc_dev->configs.gpio.dwl_req, 0);
		nfc_dev->nfc_enable_intr(nfc_dev);
	}
	nfc_dev->dev_ref_count = nfc_dev->dev_ref_count + 1;

	mutex_unlock(&nfc_dev->dev_ref_mutex);

	return 0;
}

int nfc_dev_flush(struct file *pfile, fl_owner_t id)
{
	struct nfc_dev *nfc_dev = pfile->private_data;

	if (!nfc_dev)
		return -ENODEV;
	/*
	 * release blocked user thread waiting for pending read during close
	 */
	if (!mutex_trylock(&nfc_dev->read_mutex)) {
		nfc_dev->release_read = true;
		nfc_dev->nfc_disable_intr(nfc_dev);
		wake_up(&nfc_dev->read_wq);
		pr_debug("%s: waiting for release of blocked read\n", __func__);
		mutex_lock(&nfc_dev->read_mutex);
		nfc_dev->release_read = false;
	} else {
		pr_debug("%s: read thread already released\n", __func__);
	}
	mutex_unlock(&nfc_dev->read_mutex);
	return 0;
}

int nfc_dev_close(struct inode *inode, struct file *filp)
{
	struct nfc_dev *nfc_dev = NULL;
	nfc_dev = container_of(inode->i_cdev, struct nfc_dev, c_dev);

	if (!nfc_dev)
		return -ENODEV;

	pr_debug("%s: %d, %d\n", __func__, imajor(inode), iminor(inode));

	/* unset the flag to restore to previous state */
	if (current->flags & PF_NOFREEZE) {
		current->flags &= ~PF_NOFREEZE;
		pr_debug("%s: current->flags 0x%x.\n", __func__, current->flags);
	}

	mutex_lock(&nfc_dev->dev_ref_mutex);

	if (nfc_dev->dev_ref_count == 1) {
		nfc_dev->nfc_disable_intr(nfc_dev);
		set_valid_gpio(nfc_dev->configs.gpio.dwl_req, 0);
	}

	if (nfc_dev->dev_ref_count > 0)
		nfc_dev->dev_ref_count = nfc_dev->dev_ref_count - 1;

	filp->private_data = NULL;

	mutex_unlock(&nfc_dev->dev_ref_mutex);

	return 0;
}

int is_nfc_data_available_for_read(struct nfc_dev *nfc_dev)
{
	int ret;

	nfc_dev->nfc_enable_intr(nfc_dev);

	ret = wait_event_interruptible_timeout(nfc_dev->read_wq,
			!nfc_dev->i2c_dev.irq_enabled,
			msecs_to_jiffies(MAX_IRQ_WAIT_TIME));
	return ret;
}

/**
 * get_nfcc_chip_type_dl() - get chip type in fw download command;
 * @nfc_dev:    nfc device data structure
 *
 * Perform get version command and determine chip
 * type from response.
 *
 * @Return:  enum chip_types value
 *
 */
static enum chip_types get_nfcc_chip_type_dl(struct nfc_dev *nfc_dev)
{
	int ret = 0;
	uint8_t *cmd = nfc_dev->write_kbuf;
	uint8_t *rsp = nfc_dev->read_kbuf;
	enum chip_types chip_type = CHIP_UNKNOWN;

	*cmd++ = DL_CMD;
	*cmd++ = DL_GET_VERSION_CMD_PAYLOAD_LEN;
	*cmd++ = DL_GET_VERSION_CMD_ID;
	*cmd++ = DL_PAYLOAD_BYTE_ZERO;
	*cmd++ = DL_PAYLOAD_BYTE_ZERO;
	*cmd++ = DL_PAYLOAD_BYTE_ZERO;
	*cmd++ = DL_GET_VERSION_CMD_CRC_1;
	*cmd++ = DL_GET_VERSION_CMD_CRC_2;

	pr_debug("%s:Sending GET_VERSION cmd of size = %d\n", __func__, DL_GET_VERSION_CMD_LEN);
	ret = nfc_dev->nfc_write(nfc_dev, nfc_dev->write_kbuf, DL_GET_VERSION_CMD_LEN,
									MAX_RETRY_COUNT);
	if (ret <= 0) {
		pr_err("%s: - nfc get version cmd error ret %d\n", __func__, ret);
		goto err;
	}
	memset(rsp, 0x00, DL_GET_VERSION_RSP_LEN_2);
	pr_debug("%s:Reading response of GET_VERSION cmd\n", __func__);
	ret = nfc_dev->nfc_read(nfc_dev, rsp, DL_GET_VERSION_RSP_LEN_2, NCI_CMD_RSP_TIMEOUT);
	if (ret <= 0) {
		pr_err("%s: - nfc get version rsp error ret %d\n", __func__, ret);
		goto err;
	}
	if (rsp[0] == FW_MSG_CMD_RSP && ret >= DL_GET_VERSION_RSP_LEN_2) {

		nfc_dev->fw_major_version = rsp[FW_MAJOR_VER_OFFSET];

		if (rsp[FW_ROM_CODE_VER_OFFSET] == SN1XX_ROM_VER &&
			rsp[FW_MAJOR_VER_OFFSET] == SN1xx_MAJOR_VER)
			chip_type = CHIP_SN1XX;
		else if (rsp[FW_ROM_CODE_VER_OFFSET] == SN220_ROM_VER &&
			rsp[FW_MAJOR_VER_OFFSET] == SN220_MAJOR_VER)
			chip_type = CHIP_SN220;

		pr_debug("%s:NFC Chip Type 0x%02x Rom Version 0x%02x FW Minor 0x%02x Major 0x%02x\n",
			__func__, rsp[GET_VERSION_RSP_CHIP_TYPE_OFFSET],
					rsp[FW_ROM_CODE_VER_OFFSET],
					rsp[GET_VERSION_RSP_MINOR_VERSION_OFFSET],
					rsp[FW_MAJOR_VER_OFFSET]);

		nfc_dev->nqx_info.info.chip_type = rsp[GET_VERSION_RSP_CHIP_TYPE_OFFSET];
		nfc_dev->nqx_info.info.rom_version = rsp[FW_ROM_CODE_VER_OFFSET];
		nfc_dev->nqx_info.info.fw_minor = rsp[GET_VERSION_RSP_MINOR_VERSION_OFFSET];
		nfc_dev->nqx_info.info.fw_major = rsp[FW_MAJOR_VER_OFFSET];
	}
err:
	return chip_type;
}

/**
 * get_nfcc_session_state_dl() - gets the session state
 * @nfc_dev:    nfc device data structure
 *
 * Performs get session command and determine
 * the nfcc state based on session status.
 *
 * @Return     nfcc state based on session status.
 *             NFC_STATE_FW_TEARED if sessionis not closed
 *             NFC_STATE_FW_DWL if session closed
 *             NFC_STATE_UNKNOWN in error cases.
 */
enum nfc_state_flags get_nfcc_session_state_dl(struct nfc_dev *nfc_dev)
{
	int ret = 0;
	uint8_t *cmd = nfc_dev->write_kbuf;
	uint8_t *rsp = nfc_dev->read_kbuf;
	enum nfc_state_flags nfc_state = NFC_STATE_UNKNOWN;

	*cmd++ = DL_CMD;
	*cmd++ = DL_GET_SESSION_STATE_CMD_PAYLOAD_LEN;
	*cmd++ = DL_GET_SESSION_CMD_ID;
	*cmd++ = DL_PAYLOAD_BYTE_ZERO;
	*cmd++ = DL_PAYLOAD_BYTE_ZERO;
	*cmd++ = DL_PAYLOAD_BYTE_ZERO;
	*cmd++ = DL_GET_SESSION_CMD_CRC_1;
	*cmd++ = DL_GET_SESSION_CMD_CRC_2;

	pr_debug("%s:Sending GET_SESSION_STATE cmd of size = %d\n", __func__,
						DL_GET_SESSION_STATE_CMD_LEN);
	ret = nfc_dev->nfc_write(nfc_dev, nfc_dev->write_kbuf, DL_GET_SESSION_STATE_CMD_LEN,
						MAX_RETRY_COUNT);
	if (ret <= 0) {
		pr_err("%s: - nfc get session cmd error ret %d\n", __func__, ret);
		goto err;
	}
	memset(rsp, 0x00, DL_GET_SESSION_STATE_RSP_LEN);
	pr_debug("%s:Reading response of GET_SESSION_STATE cmd\n", __func__);
	ret = nfc_dev->nfc_read(nfc_dev, rsp, DL_GET_SESSION_STATE_RSP_LEN, NCI_CMD_RSP_TIMEOUT);
	if (ret <= 0) {
		pr_err("%s: - nfc get session rsp error ret %d\n", __func__, ret);
		goto err;
	}
	if (rsp[0] != FW_MSG_CMD_RSP) {
		pr_err("%s: - nfc invalid get session state rsp\n", __func__);
		goto err;
	}
	pr_debug("Response bytes are %02x%02x%02x%02x%02x%02x%02x%02x\n",
		rsp[0], rsp[1], rsp[2], rsp[3], rsp[4], rsp[5], rsp[6], rsp[7]);
	/*verify fw in non-teared state */
	if (rsp[GET_SESSION_STS_OFF] != NFCC_SESSION_STS_CLOSED) {
		pr_err("%s NFCC booted in FW teared state\n", __func__);
		nfc_state = NFC_STATE_FW_TEARED;
	} else {
		pr_info("%s NFCC booted in FW DN mode\n", __func__);
		nfc_state = NFC_STATE_FW_DWL;
	}
err:
	return nfc_state;
}

/**
 * get_nfcc_chip_type() - get nfcc chip type in nci mode.
 * @nfc_dev:   nfc device data structure.
 *
 * Function to perform nci core reset and extract
 * chip type from the response.
 *
 * @Return:  enum chip_types value
 *
 */
static enum chip_types get_nfcc_chip_type(struct nfc_dev *nfc_dev)
{
	int ret = 0;
	uint8_t major_version = 0;
	uint8_t rom_version = 0;
	uint8_t *cmd = nfc_dev->write_kbuf;
	uint8_t *rsp = nfc_dev->read_kbuf;
	enum chip_types chip_type = CHIP_UNKNOWN;

	*cmd++ = NCI_MSG_CMD;
	*cmd++ = NCI_CORE_RESET_CMD_OID;
	*cmd++ = NCI_CORE_RESET_CMD_PAYLOAD_LEN;
	*cmd++ = NCI_CORE_RESET_KEEP_CONFIG;

	pr_debug("%s:Sending NCI Core Reset cmd of size = %d\n", __func__, NCI_RESET_CMD_LEN);
	ret = nfc_dev->nfc_write(nfc_dev, nfc_dev->write_kbuf, NCI_RESET_CMD_LEN, NO_RETRY);
	if (ret <= 0) {
		pr_err("%s: - nfc nci core reset cmd error ret %d\n", __func__, ret);
		goto err;
	}

	/* to flush out debug NTF this delay is required */
	usleep_range(NCI_RESET_RESP_READ_DELAY, NCI_RESET_RESP_READ_DELAY + 100);
	nfc_dev->nfc_enable_intr(nfc_dev);

	memset(rsp, 0x00, NCI_RESET_RSP_LEN);
	pr_debug("%s:Reading NCI Core Reset rsp\n", __func__);
	ret = nfc_dev->nfc_read(nfc_dev, rsp, NCI_RESET_RSP_LEN, NCI_CMD_RSP_TIMEOUT);
	if (ret <= 0) {
		pr_err("%s: - nfc nci core reset rsp error ret %d\n", __func__, ret);
		goto err_disable_intr;
	}

	pr_debug(" %s: nci core reset response 0x%02x%02x%02x%02x\n",
		__func__, rsp[0], rsp[1], rsp[2], rsp[3]);
	if (rsp[0] != NCI_MSG_RSP) {
		/* reset response failed response*/
		pr_err("%s invalid nci core reset response\n", __func__);
		goto err_disable_intr;
	}

	memset(rsp, 0x00, NCI_RESET_NTF_LEN);
	/* read nci rest response ntf */
	ret = nfc_dev->nfc_read(nfc_dev, rsp, NCI_RESET_NTF_LEN, NCI_CMD_RSP_TIMEOUT);
	if (ret <= 0) {
		pr_err("%s - nfc nci rest rsp ntf error status %d\n", __func__, ret);
		goto err_disable_intr;
	}

	if (rsp[0] == NCI_MSG_NTF) {
		/* read version info from NCI Reset Notification */
		rom_version = rsp[NCI_HDR_LEN + rsp[NCI_PAYLOAD_LEN_IDX] - 3];
		major_version = rsp[NCI_HDR_LEN + rsp[NCI_PAYLOAD_LEN_IDX] - 2];
		/* determine chip type based on version info */
		if (rom_version == SN1XX_ROM_VER && major_version == SN1xx_MAJOR_VER)
			chip_type = CHIP_SN1XX;
		else if (rom_version == SN220_ROM_VER && major_version == SN220_MAJOR_VER)
			chip_type = CHIP_SN220;
		pr_info(" %s:NCI  Core Reset ntf 0x%02x%02x%02x%02x\n",
			__func__, rsp[0], rsp[1], rsp[2], rsp[3]);

		nfc_dev->nqx_info.info.chip_type = rsp[NCI_HDR_LEN + rsp[NCI_PAYLOAD_LEN_IDX] -
									NFC_CHIP_TYPE_OFF];
		nfc_dev->nqx_info.info.rom_version = rom_version;
		nfc_dev->nqx_info.info.fw_major = major_version;
		nfc_dev->nqx_info.info.fw_minor = rsp[NCI_HDR_LEN + rsp[NCI_PAYLOAD_LEN_IDX] -
									NFC_FW_MINOR_OFF];
	}
err_disable_intr:
	nfc_dev->nfc_disable_intr(nfc_dev);
err:
	return chip_type;
}

/**
 * validate_download_gpio() - validate download gpio.
 * @nfc_dev: nfc_dev device data structure.
 * @chip_type: chip type of the platform.
 *
 * Validates dwnld gpio should configured for supported and
 * should not be configured for unsupported platform.
 *
 * @Return:  true if gpio validation successful ortherwise
 *           false if validation fails.
 */
static bool validate_download_gpio(struct nfc_dev *nfc_dev, enum chip_types chip_type)
{
	bool status = false;
	struct platform_gpio *nfc_gpio;

	if (nfc_dev == NULL) {
		pr_err("%s nfc devices structure is null\n");
		return status;
	}
	nfc_gpio = &nfc_dev->configs.gpio;
	if (chip_type == CHIP_SN1XX) {
		/* gpio should be configured for SN1xx */
		status = gpio_is_valid(nfc_gpio->dwl_req);
	} else if (chip_type == CHIP_SN220) {
		/* gpio should not be configured for SN220 */
		set_valid_gpio(nfc_gpio->dwl_req, 0);
		gpio_free(nfc_gpio->dwl_req);
		nfc_gpio->dwl_req = -EINVAL;
		status = true;
	}
	return status;
}

/* Check for availability of NFC controller hardware */
int nfcc_hw_check(struct nfc_dev *nfc_dev)
{
	int ret = 0;
	enum nfc_state_flags nfc_state = NFC_STATE_UNKNOWN;
	enum chip_types chip_type = CHIP_UNKNOWN;
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	/*get fw version in nci mode*/
	gpio_set_ven(nfc_dev, 1);
	gpio_set_ven(nfc_dev, 0);
	gpio_set_ven(nfc_dev, 1);
	chip_type = get_nfcc_chip_type(nfc_dev);

	/*get fw version in fw dwl mode*/
	if (chip_type == CHIP_UNKNOWN) {
		nfc_dev->nfc_enable_intr(nfc_dev);
		/*Chip is unknown, initially assume with fw dwl pin enabled*/
		set_valid_gpio(nfc_gpio->dwl_req, 1);
		gpio_set_ven(nfc_dev, 0);
		gpio_set_ven(nfc_dev, 1);
		chip_type = get_nfcc_chip_type_dl(nfc_dev);
		/*get the state of nfcc normal/teared in fw dwl mode*/
	} else {
		nfc_state = NFC_STATE_NCI;
	}

	/*validate gpio config required as per the chip*/
	if (!validate_download_gpio(nfc_dev, chip_type)) {
		pr_info("%s gpio validation fail\n", __func__);
		ret = -ENXIO;
		goto err;
	}

	/*check whether the NFCC is in FW DN or Teared state*/
	if (nfc_state != NFC_STATE_NCI)
		nfc_state = get_nfcc_session_state_dl(nfc_dev);

	/*nfcc state specific operations */
	switch (nfc_state) {
	case NFC_STATE_FW_TEARED:
		pr_warn("%s: - NFCC FW Teared State\n", __func__);
	case NFC_STATE_FW_DWL:
	case NFC_STATE_NCI:
		break;
	case NFC_STATE_UNKNOWN:
	default:
		ret = -ENXIO;
		pr_err("%s: - NFCC HW not available\n", __func__);
		goto err;
	}
	nfc_dev->nfc_state = nfc_state;
err:
	nfc_dev->nfc_disable_intr(nfc_dev);
	set_valid_gpio(nfc_gpio->dwl_req, 0);
	gpio_set_ven(nfc_dev, 0);
	gpio_set_ven(nfc_dev, 1);
	nfc_dev->nfc_ven_enabled = true;
	return ret;
}

int validate_nfc_state_nci(struct nfc_dev *nfc_dev)
{
	struct platform_gpio *nfc_gpio = &nfc_dev->configs.gpio;

	if (!gpio_get_value(nfc_gpio->ven)) {
		pr_err("VEN LOW - NFCC powered off\n");
		return -ENODEV;
	}
	if (get_valid_gpio(nfc_gpio->dwl_req) == 1) {
		pr_err("FW download in-progress\n");
		return -EBUSY;
	}
	if (nfc_dev->nfc_state != NFC_STATE_NCI) {
		pr_err("FW download state\n");
		return -EBUSY;
	}
	return 0;
}
