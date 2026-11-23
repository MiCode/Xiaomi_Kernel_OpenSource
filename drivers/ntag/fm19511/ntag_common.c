/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023-2028, The Linux Foundation. All rights reserved.
 */

#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include "ntag_common.h"

int ntag_parse_dt(struct device *dev, struct platform_configs *ntag_configs)
{
	int ret;
	struct device_node *np = dev->of_node;
	struct platform_gpio *ntag_gpio = &ntag_configs->gpio;
	struct platform_ldo *ldo = &ntag_configs->ldo;

	if (!np) {
		pr_err("NtagDrv: %s: ntag of_node NULL\n", __func__);
		return -EINVAL;
	}

	ntag_gpio->irq = -EINVAL;
	ntag_gpio->hpd = -EINVAL;

	ntag_gpio->irq = of_get_named_gpio(np, DTS_IRQ_GPIO_STR, 0);
	if ((!gpio_is_valid(ntag_gpio->irq))) {
		pr_err("NtagDrv: %s: irq gpio invalid %d\n", __func__, ntag_gpio->irq);
		return -EINVAL;
	}
	pr_info("NtagDrv: %s: irq %d\n", __func__, ntag_gpio->irq);

	// optional gpio
	ntag_gpio->hpd = of_get_named_gpio(np, DTS_HPD_GPIO_STR, 0);
	if ((!gpio_is_valid(ntag_gpio->hpd))) {
		pr_info("NtagDrv: %s: hpd gpio invalid %d\n", __func__, ntag_gpio->hpd);
	} else {
		pr_info("NtagDrv: %s: hpd %d\n", __func__, ntag_gpio->hpd);
	}

	// optional property
	ret = of_property_read_u32_array(np, NTAG_LDO_VOL_DT_NAME,
			(u32 *) ldo->vdd_levels,
			ARRAY_SIZE(ldo->vdd_levels));
	if (ret) {
		dev_err(dev, "NtagDrv: error reading NTAG VDDIO min and max value\n");
		// set default as per datasheet
		ldo->vdd_levels[0] = NTAG_VDDIO_MIN;
		ldo->vdd_levels[1] = NTAG_VDDIO_MAX;
	}

	// optional property
	ret = of_property_read_u32(np, NTAG_LDO_CUR_DT_NAME, &ldo->max_current);
	if (ret) {
		dev_err(dev, "NtagDrv: error reading NTAG current value\n");
		// set default as per datasheet
		ldo->max_current = NTAG_CURRENT_MAX;
	}

	return 0;
}

int configure_gpio(unsigned int gpio, int flag)
{
	int ret;
	pr_info("NtagDrv: %s: ntag gpio [%d] flag [%01x]\n", __func__, gpio, flag);

	if (gpio_is_valid(gpio)) {
		ret = gpio_request(gpio, "ntag_gpio");
		if (ret) {
			pr_err("NtagDrv: %s: unable to request ntag gpio [%d]\n",
					__func__, gpio);
			return ret;
		}
		/* set direction and value for output pin */
		if (flag & GPIO_OUTPUT) {
			ret = gpio_direction_output(gpio, (GPIO_HIGH & flag));
			pr_info("NtagDrv: %s: ntag o/p gpio [%d] level %d\n",
					__func__, gpio, gpio_get_value(gpio));
		} else {
			ret = gpio_direction_input(gpio);
			pr_info("NtagDrv: %s: ntag i/p gpio [%d]\n", __func__, gpio);
		}

		if (ret) {
			pr_err("NtagDrv: %s: unable to set direction for ntag gpio [%d]\n",
					__func__, gpio);
			gpio_free(gpio);
			return ret;
		}
		// Consider value as control for input IRQ pin
		if (flag & GPIO_IRQ) {
			ret = gpio_to_irq(gpio);
			if (ret < 0) {
				pr_err("NtagDrv: %s: unable to set irq for ntag gpio [%d]\n",
						__func__, gpio);
				gpio_free(gpio);
				return ret;
			}
			pr_info("NtagDrv: %s: gpio_to_irq successful for ntag gpio [%d]\n",
					__func__, gpio);
			return ret;
		}
	} else {
		pr_err("NtagDrv: %s: invalid gpio\n", __func__);
		ret = -EINVAL;
	}
	return ret;
}

int ntag_misc_register(struct ntag_dev *ntag_dev,
		const struct file_operations *ntag_fops, int count, char *devname,
		char *classname)
{
	int ret = 0;

	pr_info("NtagDrv: %s: entry\n", __func__);

	ret = alloc_chrdev_region(&ntag_dev->devno, 0, count, devname);
	if (ret < 0) {
		pr_err("%s: failed to alloc chrdev region ret %d\n",
			__func__, ret);
		return ret;
	}
	#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
	ntag_dev->ntag_class = class_create(classname);
	#else
	ntag_dev->ntag_class = class_create(THIS_MODULE, classname);
	#endif
	if (IS_ERR(ntag_dev->ntag_class)) {
		ret = PTR_ERR(ntag_dev->ntag_class);
		pr_err("NtagDrv: %s: failed to register device class ret %d\n",
				__func__, ret);
		unregister_chrdev_region(ntag_dev->devno, count);
		return ret;
	}

	cdev_init(&ntag_dev->c_dev, ntag_fops);
	ret = cdev_add(&ntag_dev->c_dev, ntag_dev->devno, count);
	if (ret < 0) {
		pr_err("NtagDrv: %s: failed to add cdev ret %d\n", __func__, ret);
		class_destroy(ntag_dev->ntag_class);
		unregister_chrdev_region(ntag_dev->devno, count);
		return ret;
	}

	ntag_dev->ntag_device = device_create(ntag_dev->ntag_class, NULL,
			ntag_dev->devno, ntag_dev, devname);
	if (IS_ERR(ntag_dev->ntag_device)) {
		ret = PTR_ERR(ntag_dev->ntag_device);
		pr_err("NtagDrv: %s: failed to create the device ret %d\n",
				__func__, ret);
		cdev_del(&ntag_dev->c_dev);
		class_destroy(ntag_dev->ntag_class);
		unregister_chrdev_region(ntag_dev->devno, count);
		return ret;
	}

	pr_info("NtagDrv: %s: exit\n", __func__);
	return 0;
}

void ntag_misc_unregister(struct ntag_dev *ntag_dev, int count)
{
	pr_info("NtagDrv: %s: entry\n", __func__);

	device_destroy(ntag_dev->ntag_class, ntag_dev->devno);
	cdev_del(&ntag_dev->c_dev);
	class_destroy(ntag_dev->ntag_class);
	unregister_chrdev_region(ntag_dev->devno, count);

	pr_info("NtagDrv: %s: exit\n", __func__);
}

int ntag_ldo_config(struct device *dev, struct ntag_dev *ntag_dev)
{
	int ret;

	if (of_get_property(dev->of_node, NTAG_LDO_SUPPLY_NAME, NULL)) {
		// Get the regulator handle
		ntag_dev->reg = regulator_get(dev, NTAG_LDO_SUPPLY_DT_NAME);
		if (IS_ERR(ntag_dev->reg)) {
			ret = PTR_ERR(ntag_dev->reg);
			ntag_dev->reg = NULL;
			pr_err("NtagDrv: %s: regulator_get failed, ret = %d\n",
					__func__, ret);
			return ret;
		}
	} else {
		ntag_dev->reg = NULL;
		pr_err("NtagDrv: %s: regulator entry not present\n", __func__);
		// return success as it's optional to configure LDO
		return 0;
	}

	// LDO config supported by platform DT
	ret = ntag_ldo_vote(ntag_dev);
	if (ret < 0) {
		pr_err("NtagDrv: %s: LDO voting failed, ret = %d\n", __func__, ret);
		regulator_put(ntag_dev->reg);
	}
	return ret;
}

int ntag_ldo_vote(struct ntag_dev *ntag_dev)
{
	int ret;

	ret = regulator_set_voltage(ntag_dev->reg,
			ntag_dev->configs.ldo.vdd_levels[0],
			ntag_dev->configs.ldo.vdd_levels[1]);
	if (ret < 0) {
		pr_err("NtagDrv: %s: set voltage failed\n", __func__);
		return ret;
	}

	/* pass expected current from NTAG in uA */
	ret = regulator_set_load(ntag_dev->reg,
			ntag_dev->configs.ldo.max_current);
	if (ret < 0) {
		pr_err("NtagDrv: %s: set load failed\n", __func__);
		return ret;
	}

	ret = regulator_enable(ntag_dev->reg);
	if (ret < 0)
		pr_err("NtagDrv: %s: regulator_enable failed\n", __func__);
	else
		ntag_dev->is_vreg_enabled = true;
	return ret;
}

int ntag_ldo_unvote(struct ntag_dev *ntag_dev)
{
	int ret;

	if (!ntag_dev->is_vreg_enabled) {
		pr_err("NtagDrv: %s: regulator already disabled\n", __func__);
		return -EINVAL;
	}

	ret = regulator_disable(ntag_dev->reg);
	if (ret < 0) {
		pr_err("NtagDrv: %s: regulator_disable failed\n", __func__);
		return ret;
	}
	ntag_dev->is_vreg_enabled = false;

	ret =  regulator_set_voltage(ntag_dev->reg, 0, NTAG_VDDIO_MAX);
	if (ret < 0) {
		pr_err("NtagDrv: %s: set voltage failed\n", __func__);
		return ret;
	}

	ret = regulator_set_load(ntag_dev->reg, 0);
	if (ret < 0)
		pr_err("NtagDrv: %s: set load failed\n", __func__);
	return ret;
}

int ntag_dev_open(struct inode *inode, struct file *filp)
{
	struct ntag_dev *ntag_dev = NULL;
	ntag_dev = container_of(inode->i_cdev, struct ntag_dev, c_dev);

	if (!ntag_dev)
		return -ENODEV;

	pr_info("NtagDrv: %s: %d, %d\n", __func__, imajor(inode), iminor(inode));

	/* Set flag to block freezer fake signal if not set already.
	 * Without this Signal being set, Driver is trying to do a read
	 * which is causing the delay in moving to Hibernate Mode.
	 */
	if (!(current->flags & PF_NOFREEZE)) {
		current->flags |= PF_NOFREEZE;
		pr_debug("NtagDrv: %s: current->flags 0x%x.\n", __func__, current->flags);
	}

	mutex_lock(&ntag_dev->dev_ref_mutex);

	filp->private_data = ntag_dev;

	ntag_dev->dev_ref_count = ntag_dev->dev_ref_count + 1;

	mutex_unlock(&ntag_dev->dev_ref_mutex);

	return 0;
}

int ntag_dev_close(struct inode *inode, struct file *filp)
{
	struct ntag_dev *ntag_dev = NULL;
	ntag_dev = container_of(inode->i_cdev, struct ntag_dev, c_dev);

	if (!ntag_dev)
		return -ENODEV;

	pr_info("NtagDrv: %s: %d, %d\n", __func__, imajor(inode), iminor(inode));

	/* unset the flag to restore to previous state */
	if (current->flags & PF_NOFREEZE) {
		current->flags &= ~PF_NOFREEZE;
		pr_debug("NtagDrv: %s: current->flags 0x%x.\n", __func__, current->flags);
	}

	mutex_lock(&ntag_dev->dev_ref_mutex);

	if (ntag_dev->dev_ref_count > 0)
		ntag_dev->dev_ref_count = ntag_dev->dev_ref_count - 1;

	filp->private_data = NULL;

	mutex_unlock(&ntag_dev->dev_ref_mutex);

	if (ntag_dev->fasync_queue) {
		ntag_irq_fasync(-1, filp, 0);
	}

	return 0;
}

int ntag_irq_fasync(int fd, struct file *pfile, int on)
{
	int ret = 0;
	struct ntag_dev *ntag_dev = pfile->private_data;

	if (!ntag_dev)
		return -ENODEV;

	ret = fasync_helper(fd, pfile, on, &(ntag_dev->fasync_queue));
	if (ret < 0) {
		pr_err("NtagDrv: %s: on = %d, failed\n", __func__, on);
	} else {
		pr_info("NtagDrv: %s: on = %d, success\n", __func__, on);
	}
	return ret;
}

int read_eeprom(struct ntag_dev *ntag_dev,
		uint16_t addr, uint8_t* read_buf, uint16_t count)
{
	int ret = 0;
	unsigned char buf[2] = {0x00};

	pr_debug("NtagDrv: %s: addr 0x%04X, count = %d\n", __func__, addr, count);
	buf[0] = addr >> 8;
	buf[1] = addr & 0xFF;

	ret = ntag_dev->ntag_write(ntag_dev, buf, 2);
	if (ret <= 0) {
		return ret;
	}
	ret = ntag_dev->ntag_read(ntag_dev, read_buf, count);
	if (ret <= 0) {
		return ret;
	}
	pr_debug("NtagDrv: %s: read success\n", __func__);

	return ret;
}

int write_eeprom(struct ntag_dev *ntag_dev,
		uint16_t addr, uint8_t* write_buf, uint8_t buf_len)
{
	int ret = 0;
	unsigned char buf[4] = {0x00};

	if (buf_len > 2) {
		pr_err("NtagDrv: %s: invalid buf length\n", __func__);
		return -EINVAL;
	}

	pr_debug("NtagDrv: %s: addr 0x%04X, buf_len = %d\n", __func__, addr, buf_len);
	buf[0] = addr >> 8;
	buf[1] = addr & 0xFF;
	memcpy(buf + 2, write_buf, buf_len);

	ret = ntag_dev->ntag_write(ntag_dev, buf, 2 + buf_len);
	if (ret <= 0) {
		return ret;
	}
	pr_debug("NtagDrv: %s: write success\n", __func__);

	usleep_range(5000, 5100);
	return ret;
}

int ntag_read_register(struct ntag_dev *ntag_dev,
		uint16_t reg_addr, uint8_t* read_buf)
{
	pr_info("NtagDrv: %s: reg addr 0x%04X\n", __func__, reg_addr);
	return read_eeprom(ntag_dev, reg_addr, read_buf, 1);
}

int ntag_write_register(struct ntag_dev *ntag_dev,
		uint16_t reg_addr, uint8_t reg_data)
{
	pr_info("NtagDrv: %s: reg addr 0x%04X\n", __func__, reg_addr);
	return write_eeprom(ntag_dev, reg_addr, &reg_data, 1);
}

int ntag_read_block(struct ntag_dev *ntag_dev,
		uint8_t block_addr, uint8_t block_num, uint8_t* read_buf)
{
	int ret = 0;

	pr_info("NtagDrv: %s: block addr 0x%02X, block num = %d\n",
			__func__, block_addr, block_num);

	ret = read_eeprom(ntag_dev, block_addr*4, read_buf, block_num*4);
	if (ret <= 0) {
		return ret;
	}
	pr_info("NtagDrv: %s: read success\n", __func__);
	return ret;
}

int ntag_write_block(struct ntag_dev *ntag_dev,
		uint8_t block_addr, uint8_t* write_buf, uint8_t buf_len)
{
	int ret = 0;

	pr_info("NtagDrv: %s: block addr 0x%02X, buf len = %d\n",
			__func__, block_addr, buf_len);
	if (buf_len != NTAG_BLOCK_SIZE) {
		pr_err("NtagDrv: %s: Warning! buf len must equal %d\n", __func__, NTAG_BLOCK_SIZE);
	}

	ret = write_eeprom(ntag_dev, block_addr*4, write_buf, 2);
	if (ret <= 0) {
		return ret;
	}
	ret = write_eeprom(ntag_dev, block_addr*4 + 2, write_buf + 2, 2);
	if (ret <= 0) {
		return ret;
	}
	pr_info("NtagDrv: %s: write success\n", __func__);

	return ret;
}

// read any register to wake up chip
void wakeup_chip(struct ntag_dev *ntag_dev)
{
    unsigned char temp = 0x00;
    read_eeprom(ntag_dev, 0xFFFF, &temp, 1);
}

void disable_auto_standby(struct ntag_dev *ntag_dev)
{
	unsigned char low_power_reg_val = 0x00;
	int i;

	pr_info("NtagDrv: %s: enter\n", __func__);
	for (i = 0; i < 5; i++) {
		ntag_write_register(ntag_dev, LOW_POWER_CFG_REG, 0x01);
		ntag_read_register(ntag_dev, LOW_POWER_CFG_REG, &low_power_reg_val);
		if (low_power_reg_val == 0x01) {
			pr_info("NtagDrv: %s: succeed", __func__);
			return;
		}
	}
	if (low_power_reg_val != 0x01) {
		pr_err("NtagDrv: %s: failed", __func__);
	}
}

int check_cfg_register_ee(struct ntag_dev *ntag_dev)
{
	int ret = 0;
	unsigned char reg_ee_val_read = 0x00;

	pr_info("NtagDrv: %s: enter\n", __func__);
	// LOW_POWER_CFG_REG_EE
	ret = ntag_read_register(ntag_dev,
			LOW_POWER_CFG_REG_EE, &reg_ee_val_read);
	if (ret <= 0) {
		return ret;
	}
	if (reg_ee_val_read != LOW_POWER_CFG_REG_VAL) {
		ret = ntag_write_register(ntag_dev,
				LOW_POWER_CFG_REG_EE, LOW_POWER_CFG_REG_VAL);
		if (ret <= 0) {
			return ret;
		}
	}
	// MASK_MAIN_IRQ_REG_EE
	ret = ntag_read_register(ntag_dev,
			MASK_MAIN_IRQ_REG_EE, &reg_ee_val_read);
	if (ret <= 0) {
		return ret;
	}
	if (reg_ee_val_read != MASK_MAIN_IRQ_REG_VAL) {
		ret = ntag_write_register(ntag_dev,
				MASK_MAIN_IRQ_REG_EE, MASK_MAIN_IRQ_REG_VAL);
		if (ret <= 0) {
			return ret;
		}
	}
	// MASK_AUX_IRQ_REG_EE
	ret = ntag_read_register(ntag_dev,
			MASK_AUX_IRQ_REG_EE, &reg_ee_val_read);
	if (ret <= 0) {
		return ret;
	}
	if (reg_ee_val_read != MASK_AUX_IRQ_REG_VAL) {
		ret = ntag_write_register(ntag_dev,
				MASK_AUX_IRQ_REG_EE, MASK_AUX_IRQ_REG_VAL);
		if (ret <= 0) {
			return ret;
		}
	}
	// IRQ_CFG_REG_EE
	ret = ntag_read_register(ntag_dev,
			IRQ_CFG_REG_EE, &reg_ee_val_read);
	if (ret <= 0) {
		return ret;
	}
	if (reg_ee_val_read != IRQ_CFG_REG_VAL) {
		ret = ntag_write_register(ntag_dev,
				IRQ_CFG_REG_EE, IRQ_CFG_REG_VAL);
		if (ret <= 0) {
			return ret;
		}
	}

	// write CHIP_CTRL_REG: Soft_Reset and CFG_Reload
	pr_info("NtagDrv: %s: soft reset\n", __func__);
	ret = ntag_write_register(ntag_dev, CHIP_CTRL_REG, CHIP_CTRL_REG_VAL);
	if (ret <= 0) {
		return ret;
	}
	// sleep 1ms for config reload done
	usleep_range(1000, 1100);

	pr_info("NtagDrv: %s: exit\n", __func__);
	return ret;
}

int check_cfg_register(struct ntag_dev *ntag_dev)
{
	int ret = 0;
	unsigned char reg_val_read = 0x00;

	pr_info("NtagDrv: %s: enter\n", __func__);
	// LOW_POWER_CFG_REG
	if (ntag_read_register(ntag_dev, LOW_POWER_CFG_REG, &reg_val_read) <= 0
			|| reg_val_read != LOW_POWER_CFG_REG_VAL) {
		return -1;
	}
	// MASK_MAIN_IRQ_REG
	if (ntag_read_register(ntag_dev, MASK_MAIN_IRQ_REG, &reg_val_read) <= 0
			|| reg_val_read != MASK_MAIN_IRQ_REG_VAL) {
		return -1;
	}
	// MASK_AUX_IRQ_REG
	if (ntag_read_register(ntag_dev, MASK_AUX_IRQ_REG, &reg_val_read) <= 0
			|| reg_val_read != MASK_AUX_IRQ_REG_VAL) {
		return -1;
	}
	// IRQ_CFG_REG
	if (ntag_read_register(ntag_dev, IRQ_CFG_REG, &reg_val_read) <= 0
			|| reg_val_read != IRQ_CFG_REG_VAL) {
		return -1;
	}

	pr_info("NtagDrv: %s: exit\n", __func__);
	return ret;
}

// clear irq register
void ntag_clear_irq_register(struct ntag_dev *ntag_dev)
{
	struct platform_gpio *ntag_gpio = &ntag_dev->configs.gpio;
	uint8_t retry_max = 3;
	unsigned char temp = 0x00;

	pr_info("NtagDrv: %s\n", __func__);
	do {
		wakeup_chip(ntag_dev);
		read_eeprom(ntag_dev, MAIN_IRQ_REG, &temp, 1);
		read_eeprom(ntag_dev, FIFO_IRQ_REG, &temp, 1);
		read_eeprom(ntag_dev, AUX_IRQ_REG, &temp, 1);

		usleep_range(1000, 1100);
		if (gpio_get_value(ntag_gpio->irq)) {
			break;
		} else {
			pr_info("NtagDrv: %s: gpio irq not high, retry\n", __func__);
		}
	} while (--retry_max);
}

// 重置芯片、检查配置寄存器, 清除中断寄存器
int ntag_init(struct ntag_dev *ntag_dev)
{
	int ret = 0;

	unsigned char read_buf[MAX_BUFFER_SIZE] = {0x00};

	struct platform_gpio *ntag_gpio = &ntag_dev->configs.gpio;

	pr_info("NtagDrv: %s: enter\n", __func__);

	// Hard Reset chip
	if (gpio_is_valid(ntag_gpio->hpd)) {
		gpio_set_value(ntag_gpio->hpd, 1);
		usleep_range(5000, 5100);
		gpio_set_value(ntag_gpio->hpd, 0);
		usleep_range(5000, 5100);
		gpio_set_value(ntag_gpio->hpd, 1);
		usleep_range(5000, 5100);
	}

	wakeup_chip(ntag_dev);
	disable_auto_standby(ntag_dev);

	pr_info("NtagDrv: %s: read block memory\n", __func__);
	ntag_read_block(ntag_dev, 0x00, 40, read_buf);

	pr_info("NtagDrv: %s: check cfg registers ee\n", __func__);
	check_cfg_register_ee(ntag_dev);

	wakeup_chip(ntag_dev);
	pr_info("NtagDrv: %s: check cfg registers\n", __func__);
	ret = check_cfg_register(ntag_dev);

	pr_info("NtagDrv: %s: clear irq registers\n", __func__);
	ntag_clear_irq_register(ntag_dev);

	pr_info("NtagDrv: %s: exit, ret %d\n", __func__, ret);
	return ret;
}
