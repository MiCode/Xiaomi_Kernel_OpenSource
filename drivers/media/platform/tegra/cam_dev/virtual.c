/*
 * virtual.c - Virtual kernel driver
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define CAMERA_DEVICE_INTERNAL

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/clk.h>

#include <media/nvc.h>
#include <media/camera.h>

struct chip_config {
	int clk_num;
	int gpio_num;
	int reg_num;
	struct camera_reg *seq_power_on;
	struct camera_reg *seq_power_off;
	char *reg_names[];
};

static int virtual_update(
	struct camera_device *cdev, struct cam_update *upd, int num)
{
	int err = 0;
	int idx;

	dev_dbg(cdev->dev, "%s %d\n", __func__, num);
	mutex_lock(&cdev->mutex);
	for (idx = 0; idx < num; idx++) {
		switch (upd[idx].type) {
		case UPDATE_EDP:
		{
			struct edp_cfg ec;
			struct edp_client *pec = &cdev->edpc.edp_client;

			/* update edp throttle seq */
			if (upd[idx].index == CAMERA_SEQ_FLAG_EDP) {
				u32 sidx = upd[idx].arg;
				dev_dbg(cdev->dev, "%s UPDATE_EDP throttle %d\n",
					__func__, sidx);
				if (sidx >= NUM_OF_SEQSTACK ||
					!cdev->seq_stack[sidx]) {
					dev_err(cdev->dev, "edp index err!\n");
					err = -ENOENT;
					break;
				}

				cdev->edpc.s_throttle = cdev->seq_stack[sidx];
				break;
			}

			dev_dbg(cdev->dev, "%s UPDATE_EDP config\n", __func__);
			if (cdev->edpc.edpc_en) {
				dev_err(cdev->dev, "edp client already set!\n");
				err = -EEXIST;
				break;
			}
			if (upd[idx].size != sizeof(ec)) {
				dev_err(cdev->dev, "Invalid edp cfg size!\n");
				err = -EINVAL;
				break;
			}
			memset(&ec, 0, sizeof(ec));
			if (copy_from_user(&ec,
				((const void __user *)
				(unsigned long)upd[idx].arg),
				sizeof(ec))) {
				dev_err(cdev->dev,
					"%s copy_from_user err line %d\n",
					__func__, __LINE__);
				err = -EFAULT;
				break;
			}
			if (ec.num > CAMERA_MAX_EDP_ENTRIES) {
				dev_err(cdev->dev, "too many estate entries!\n");
				err = -E2BIG;
				break;
			}

			memcpy(cdev->estates, ec.estates,
				ec.num * sizeof(cdev->estates[0]));
			pec->states = cdev->estates;
			pec->num_states = ec.num;
			pec->e0_index = ec.e0_index;
			pec->priority = ec.priority;
			camera_edp_register(cdev);
			break;
		}
		case UPDATE_CLOCK:
		{
			struct clk *ck;
			u8 buf[CAMERA_MAX_NAME_LENGTH];

			if (!cdev->num_clk) {
				dev_err(cdev->dev, "NO clock needed.\n");
				err = -ENODEV;
				break;
			}
			if (upd[idx].index >= cdev->num_clk) {
				dev_err(cdev->dev,
					"clock index %d out of range.\n",
					upd[idx].index);
				err = -ENODEV;
				break;
			}

			memset(buf, 0, sizeof(buf));
			if (copy_from_user(buf,
				((const void __user *)
				(unsigned long)upd[idx].arg),
				sizeof(buf) - 1 < upd[idx].size ?
				sizeof(buf) - 1 : upd[idx].size)) {
				dev_err(cdev->dev,
					"%s copy_from_user err line %d\n",
					__func__, __LINE__);
				err = -EFAULT;
				break;
			}

			dev_dbg(cdev->dev, "%s UPDATE_CLOCK %d of %d, %s\n",
				__func__, upd[idx].index, cdev->num_clk, buf);
			ck = devm_clk_get(cdev->dev, buf);
			if (IS_ERR(ck)) {
				dev_err(cdev->dev, "%s: get clock %s FAILED.\n",
					__func__, buf);
				return PTR_ERR(ck);
			}
			cdev->clks[upd[idx].index] = ck;
			dev_dbg(cdev->dev, "UPDATE_CLOCK: %d %s\n",
				upd[idx].index, buf);
			break;
		}
		case UPDATE_PINMUX:
		{
			u32 *pinmux;

			if (!cdev->pinmux_num) {
				dev_err(cdev->dev, "NO pinmux available.\n");
				err = -ENODEV;
				break;
			}
			if (upd[idx].arg >= cdev->pinmux_num) {
				dev_err(cdev->dev,
					"pinmux index %u out of range.\n",
					upd[idx].arg);
				err = -ENODEV;
				break;
			}

			dev_dbg(cdev->dev, "UPDATE_PINMUX: %d %u\n",
				upd[idx].index, upd[idx].arg);
			if (!upd[idx].index)
				pinmux = &cdev->mclk_enable_idx;
			else
				pinmux = &cdev->mclk_disable_idx;
			*pinmux = upd[idx].arg;
			break;
		}
		case UPDATE_GPIO:
		{
			struct nvc_gpio *gpio;

			if (upd[idx].index >= cdev->num_gpio) {
				dev_err(cdev->dev,
					"gpio index %d out of range.\n",
					upd[idx].index);
				err = -ENODEV;
				break;
			}
			gpio = (void *)((unsigned long)upd[idx].arg);
			if (gpio->gpio >= ARCH_NR_GPIOS) {
				dev_err(cdev->dev,
					"gpio index %d out of range.\n",
					gpio->gpio);
				err = -ENODEV;
				break;
			}

			dev_dbg(cdev->dev, "UPDATE_GPIO: %d %u\n",
				upd[idx].index, upd[idx].arg);
			gpio->valid = true;
			cdev->gpios[upd[idx].index] = *gpio;
			break;
		}
		default:
			dev_err(cdev->dev,
				"unsupported upd type %d\n", upd[idx].type);
			break;
		}

		if (err)
			break;
	}
	mutex_unlock(&cdev->mutex);
	return err;
}

static int virtual_power_on(struct camera_device *cdev)
{
	struct chip_config *c_info = cdev->chip->private;
	struct camera_reg *pwr_seq = c_info->seq_power_on;
	int err = 0;

	dev_dbg(cdev->dev, "%s %x %p\n",
		__func__, cdev->is_power_on, pwr_seq);
	if (cdev->is_power_on || !pwr_seq)
		return 0;

	mutex_lock(&cdev->mutex);
	err = camera_dev_wr_table(cdev, pwr_seq, NULL);
	if (!err)
		cdev->is_power_on = 1;
	mutex_unlock(&cdev->mutex);
	return err;
}

static int virtual_power_off(struct camera_device *cdev)
{
	struct chip_config *c_info = cdev->chip->private;
	struct camera_reg *pwr_seq = c_info->seq_power_off;
	int err = 0;

	dev_dbg(cdev->dev, "%s %x %p\n",
		__func__, cdev->is_power_on, pwr_seq);
	if (!cdev->is_power_on || !pwr_seq)
		return 0;

	mutex_lock(&cdev->mutex);
	err = camera_dev_wr_table(cdev, pwr_seq, NULL);
	if (!err)
		cdev->is_power_on = 0;
	mutex_unlock(&cdev->mutex);

	return err;
}

static int virtual_shutdown(struct camera_device *cdev)
{
	struct camera_reg *t_seq = cdev->edpc.s_throttle;
	int err = 0;

	dev_dbg(cdev->dev, "%s %x %p\n",
		__func__, cdev->is_power_on, t_seq);
	if (!cdev->is_power_on)
		return 0;

	if (t_seq) {
		mutex_lock(&cdev->mutex);
		err = camera_dev_wr_table(cdev, t_seq, NULL);
		mutex_unlock(&cdev->mutex);
	}

	if (!err)
		err = virtual_power_off(cdev);

	return err;
}

static int virtual_instance_destroy(struct camera_device *cdev)
{
	void *buf;
	u32 idx;

	dev_dbg(cdev->dev, "%s\n", __func__);

	if (!cdev->dev)
		return 0;

	buf = dev_get_drvdata(cdev->dev);
	dev_set_drvdata(cdev->dev, NULL);

	for (idx = 0; idx < cdev->num_reg; idx++)
		if (likely(cdev->regs[idx].vreg))
			regulator_put(cdev->regs[idx].vreg);

	cdev->num_gpio = 0;
	cdev->num_reg = 0;
	kfree(buf);
	return 0;
}

static int virtual_instance_create(struct camera_device *cdev, void *pdata)
{
	struct chip_config *c_info = cdev->chip->private;
	u32 idx;

	dev_dbg(cdev->dev, "%s\n", __func__);
	cdev->gpios = kzalloc(c_info->gpio_num * sizeof(*cdev->gpios) +
		c_info->reg_num * sizeof(*cdev->regs) +
		c_info->clk_num * sizeof(*cdev->clks),
		GFP_KERNEL);
	if (cdev->gpios == NULL) {
		dev_err(cdev->dev, "%s memory low!\n", __func__);
		return -ENOMEM;
	}

	cdev->pinmux_num = ((struct camera_platform_data *)pdata)->pinmux_num;
	cdev->pinmux_tbl = ((struct camera_platform_data *)pdata)->pinmux;
	cdev->num_gpio = c_info->gpio_num;
	cdev->regs = (void *)cdev->gpios +
		c_info->gpio_num * sizeof(*cdev->gpios);
	cdev->num_reg = c_info->reg_num;
	cdev->clks = (void *)cdev->regs + c_info->reg_num * sizeof(*cdev->regs);
	cdev->num_clk = c_info->clk_num;
	cdev->mclk_enable_idx = CAMDEV_INVALID;
	cdev->mclk_disable_idx = CAMDEV_INVALID;

	for (idx = 0; idx < cdev->num_gpio; idx++)
		cdev->gpios[idx].valid = false;

	for (idx = 0; idx < cdev->num_reg; idx++) {
		cdev->regs[idx].vreg_name =
			(const char *)c_info->reg_names[idx];
		camera_regulator_get(cdev->dev, &cdev->regs[idx],
			(char *)cdev->regs[idx].vreg_name);
	}

	dev_set_drvdata(cdev->dev, cdev->gpios);
	return 0;
}

static struct regmap_config regmap_cfg_default = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static int virtual_device_sanity_check(
	struct device *dev, struct virtual_device *dev_info, int *len)
{
	u32 num;
	u8 *nptr;
	int n;

	dev_dbg(dev, "%s: %s, bus type %d, addr bits %d, val bits %d\n",
		__func__, dev_info->name, dev_info->bus_type,
		dev_info->regmap_cfg.addr_bits, dev_info->regmap_cfg.val_bits);
	dev_dbg(dev, "gpios %d, regs %d, clks %d\n",
		dev_info->gpio_num, dev_info->reg_num, dev_info->clk_num);
	if (dev_info->name[0] == '\0') {
		dev_err(dev, "%s need a device name!\n", __func__);
		return -ENODEV;
	}

	if (dev_info->bus_type != CAMERA_DEVICE_TYPE_I2C) {
		dev_err(dev, "%s unsupported device type %d!\n",
		__func__, dev_info->bus_type);
		return -ENODEV;
	}

	if (dev_info->regmap_cfg.addr_bits != 0 &&
		dev_info->regmap_cfg.addr_bits != 8 &&
		dev_info->regmap_cfg.addr_bits != 16) {
		dev_err(dev, "%s unsupported address bits %d!\n",
		__func__, dev_info->regmap_cfg.addr_bits);
		return -ENODEV;
	}

	if (dev_info->regmap_cfg.val_bits != 8 &&
		dev_info->regmap_cfg.val_bits != 16) {
		dev_err(dev, "%s unsupported data bits %d!\n",
		__func__, dev_info->regmap_cfg.val_bits);
		return -ENODEV;
	}

	if (dev_info->regmap_cfg.cache_type != REGCACHE_NONE &&
		dev_info->regmap_cfg.cache_type != REGCACHE_RBTREE &&
		dev_info->regmap_cfg.cache_type != REGCACHE_COMPRESSED) {
		dev_err(dev, "%s unsupported cache type %d!\n",
		__func__, dev_info->regmap_cfg.cache_type);
		return -ENODEV;
	}

	if (dev_info->gpio_num >= ARCH_NR_GPIOS) {
		dev_err(dev, "%s too many gpios %d!\n",
		__func__, dev_info->gpio_num);
		return -ENODEV;
	}

	if (dev_info->gpio_num >= 6) {
		dev_notice(dev, "%s WHAT?! Are you sure you need %d gpios?\n",
			__func__, dev_info->gpio_num);
	}

	if (dev_info->clk_num >= 5) {
		dev_notice(dev, "%s WHAT?! Are you sure you need %d clocks?\n",
			__func__, dev_info->clk_num);
	}

	*len = 0;
	num = dev_info->reg_num;
	nptr = &dev_info->reg_names[0];
	while (num) {
		n = strlen(nptr);
		if (!n) {
			dev_err(dev, "%s NULL reg name @ %d\n",
				__func__, dev_info->reg_num - num);
			return -ENODEV;
		}
		*len += n + 1;
		nptr += CAMERA_MAX_NAME_LENGTH;
		num--;
	}
	dev_dbg(dev, "regulator name size: %d\n", *len);

	return 0;
}

static int virtual_chip_config(
	struct device *dev,
	struct virtual_device *dev_info,
	struct chip_config *c_info)
{
	char *rptr = (void *)c_info;
	char *nptr = dev_info->reg_names;
	int len;
	u32 idx;

	dev_dbg(dev, "%s regulators:\n", __func__);
	c_info->clk_num = dev_info->clk_num;
	c_info->gpio_num = dev_info->gpio_num;
	c_info->reg_num = dev_info->reg_num;
	rptr += sizeof(*c_info) + sizeof(char *) * c_info->reg_num;
	for (idx = 0; idx < c_info->reg_num; idx++) {
		c_info->reg_names[idx] = rptr;
		len = strlen(nptr);
		dev_dbg(dev, "#%d %s len %d\n", idx, nptr, len);
		strcpy(rptr, nptr);
		rptr[len] = '\0';
		rptr += len + 1;
		nptr += CAMERA_MAX_NAME_LENGTH;
		dev_dbg(dev, "#%d - %s\n", idx, c_info->reg_names[idx]);
	}

	c_info->seq_power_on = (void *)rptr;
	if (copy_from_user(
		c_info->seq_power_on,
		(const void __user *)(unsigned long)dev_info->power_on,
		sizeof(struct camera_reg) * dev_info->pwr_on_size)) {
		dev_err(dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
			return -EFAULT;
	}

	c_info->seq_power_off = (void *)c_info->seq_power_on +
		sizeof(struct camera_reg) * dev_info->pwr_on_size;
	if (copy_from_user(
		c_info->seq_power_off,
		(const void __user *)(unsigned long)dev_info->power_off,
		sizeof(struct camera_reg) * dev_info->pwr_off_size)) {
		dev_err(dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
			return -EFAULT;
	}

	return 0;
}

int virtual_device_add(struct device *dev, unsigned long arg)
{
	struct virtual_device dev_info;
	struct camera_chip *v_chip;
	struct chip_config *c_info;
	struct regmap_config *p_regmap;
	int buf_len;
	int err = 0;

	dev_info(dev, "%s\n", __func__);

	if (copy_from_user(
		&dev_info, (const void __user *)arg, sizeof(dev_info))) {
		dev_err(dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
			return -EFAULT;
	}

	err = virtual_device_sanity_check(dev, &dev_info, &buf_len);
	if (err)
		return err;

	buf_len += sizeof(char *) * dev_info.reg_num +
		sizeof(struct camera_reg) * dev_info.pwr_on_size +
		sizeof(struct camera_reg) * dev_info.pwr_off_size;
	v_chip = kzalloc(
		sizeof(*v_chip) + sizeof(*c_info) + buf_len, GFP_KERNEL);
	if (!v_chip) {
		dev_err(dev, "%s unable to allocate memory!\n", __func__);
		return -ENOMEM;
	}

	c_info = (void *)v_chip + sizeof(*v_chip);
	err = virtual_chip_config(dev, &dev_info, c_info);
	if (err) {
		kfree(v_chip);
		return err;
	}

	strncpy((u8 *)v_chip->name, (u8 const *)dev_info.name,
		sizeof(v_chip->name));

	p_regmap = (struct regmap_config *)&v_chip->regmap_cfg;
	memcpy(p_regmap, &regmap_cfg_default, sizeof(*p_regmap));
	v_chip->type = dev_info.bus_type;
	if (dev_info.regmap_cfg.addr_bits)
		p_regmap->reg_bits = dev_info.regmap_cfg.addr_bits;
	if (dev_info.regmap_cfg.val_bits)
		p_regmap->val_bits = dev_info.regmap_cfg.val_bits;
	p_regmap->cache_type = dev_info.regmap_cfg.cache_type;

	INIT_LIST_HEAD(&v_chip->list);
	v_chip->private = c_info;
	v_chip->init = virtual_instance_create;
	v_chip->release = virtual_instance_destroy,
	v_chip->power_on = virtual_power_on,
	v_chip->power_off = virtual_power_off,
	v_chip->shutdown = virtual_shutdown,
	v_chip->update = virtual_update,

	camera_chip_add(v_chip);
	return 0;
}

static int __init virtual_init(void)
{
	pr_info("%s\n", __func__);
	return 0;
}
device_initcall(virtual_init);

static void __exit virtual_exit(void)
{
}
module_exit(virtual_exit);

MODULE_DESCRIPTION("virtual sensor device");
MODULE_AUTHOR("Charlie Huang <chahuang@nvidia.com>");
MODULE_LICENSE("GPL v2");
