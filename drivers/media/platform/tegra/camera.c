/*
 * camera.c - generic camera device driver
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Contributors:
 *	Charlie Huang <chahuang@nvidia.com>
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

/* Implementation
 * --------------
 * The board level details about the device are to be provided in the board
 * file with the <device>_platform_data structure.
 * Standard among NVC kernel drivers in this structure is:
 * .dev_name = The MISC driver name the device registers as.  If not used,
 *	     then the part number of the device is used for the driver name.
 *	     If using the NVC user driver then use the name found in this
 *	     driver under _default_pdata.
 */

#define CAMERA_DEVICE_INTERNAL
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/clk.h>

#include <media/camera.h>

static struct camera_platform_data camera_dflt_pdata = {
	.cfg = 0,
};

static DEFINE_MUTEX(app_mutex);
static DEFINE_MUTEX(dev_mutex);
static DEFINE_MUTEX(chip_mutex);
static LIST_HEAD(app_list);
static LIST_HEAD(dev_list);
static LIST_HEAD(chip_list);

static struct camera_platform_info cam_desc = {
	.in_use = ATOMIC_INIT(0),
	.u_mutex = &app_mutex,
	.d_mutex = &dev_mutex,
	.c_mutex = &chip_mutex,
	.app_list = &app_list,
	.dev_list = &dev_list,
	.chip_list = &chip_list,
};

static int camera_seq_rd(struct camera_info *cam, unsigned long arg)
{
	struct nvc_param params;
	struct camera_reg *p_i2c_table;
	int err;

	dev_dbg(cam->dev, "%s %lx\n", __func__, arg);
	if (copy_from_user(&params, (const void __user *)arg,
		sizeof(struct nvc_param))) {
		dev_err(cam->dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
		return -EFAULT;
	}

	p_i2c_table = kzalloc(sizeof(params.sizeofvalue), GFP_KERNEL);
	if (p_i2c_table == NULL) {
		dev_err(cam->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(p_i2c_table, (const void __user *)params.p_value,
		params.sizeofvalue)) {
		dev_err(cam->dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
		kfree(p_i2c_table);
		return -EINVAL;
	}

	err = camera_dev_rd_table(cam->cdev, p_i2c_table);
	if (!err && copy_to_user((void __user *)params.p_value,
		p_i2c_table, params.sizeofvalue)) {
		dev_err(cam->dev, "%s copy_to_user err line %d\n",
			__func__, __LINE__);
		err = -EINVAL;
	}

	kfree(p_i2c_table);
	return err;
}

static int camera_seq_wr(struct camera_info *cam, unsigned long arg)
{
	struct nvc_param params;
	struct camera_device *cdev = cam->cdev;
	struct camera_reg *p_i2c_table = NULL;
	struct camera_seq_status seqs;
	u8 pfree = 0;
	int err = 0;
	int idx;

	dev_dbg(cam->dev, "%s %lx", __func__, arg);

	if (copy_from_user(&params, (const void __user *)arg,
		sizeof(struct nvc_param))) {
		dev_err(cam->dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
		return -EFAULT;
	}

	dev_dbg(cam->dev, "param: %x, size %d\n", params.param,
		params.sizeofvalue);
	if (params.param == CAMERA_SEQ_EXIST) {
		idx = params.variant & CAMERA_SEQ_INDEX_MASK;
		if (idx >= NUM_OF_SEQSTACK) {
			dev_err(cam->dev, "%s seq index out of range %d\n",
				__func__, idx);
			return -EFAULT;
		}
		p_i2c_table = cdev->seq_stack[idx];
		if (p_i2c_table == NULL) {
			dev_err(cam->dev, "%s seq index empty! %d\n",
				__func__, idx);
			return -EEXIST;
		}
		goto seq_wr_table;
	}

	p_i2c_table = devm_kzalloc(cdev->dev, params.sizeofvalue, GFP_KERNEL);
	if (p_i2c_table == NULL) {
		dev_err(cam->dev, "%s devm_kzalloc err line %d\n",
			__func__, __LINE__);
		return -ENOMEM;
	}
	pfree = 1;

	if (copy_from_user(p_i2c_table,
		(const void __user *)params.p_value, params.sizeofvalue)) {
		dev_err(cam->dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
		err = -EFAULT;
		goto seq_wr_end;
	}

	switch (params.param) {
	case CAMERA_SEQ_REGISTER_EXEC:
	case CAMERA_SEQ_REGISTER_ONLY:
		for (idx = 0; idx < NUM_OF_SEQSTACK; idx++) {
			if (!cdev->seq_stack[idx]) {
				cdev->seq_stack[idx] = p_i2c_table;
				pfree = 0;
				break;
			}
		}
		if (idx >= NUM_OF_SEQSTACK) {
			dev_err(cam->dev, "%s seq index full!\n", __func__);
			err = -EINVAL;
			goto seq_wr_end;
		} else {
			if (params.variant & CAMERA_SEQ_FLAG_EDP)
				cdev->edpc.s_throttle = p_i2c_table;
			params.variant = idx;
			goto seq_wr_upd;
		}
		if (params.param == CAMERA_SEQ_REGISTER_EXEC)
			goto seq_wr_table;
		break;
	case CAMERA_SEQ_EXEC:
		break;
	}

seq_wr_table:
	if (err < 0)
		goto seq_wr_end;

	mutex_lock(&cdev->mutex);
	err = camera_dev_wr_table(cdev, p_i2c_table, &seqs);
	mutex_unlock(&cdev->mutex);
	if (err < 0) {
		params.param = CAMERA_SEQ_STATUS_MASK | seqs.idx;
		params.variant = seqs.status;
	}

seq_wr_upd:
	if (copy_to_user((void __user *)arg,
		(const void *)&params, sizeof(params))) {
		dev_err(cam->dev, "%s copy_to_user err line %d\n",
			__func__, __LINE__);
		err = -EFAULT;
	}

seq_wr_end:
	if (pfree)
		devm_kfree(cdev->dev, p_i2c_table);
	return err;
}

static int camera_dev_pwr_set(struct camera_info *cam, unsigned long pwr)
{
	struct camera_device *cdev = cam->cdev;
	struct camera_chip *chip = cdev->chip;
	int err = 0;

	dev_dbg(cam->dev, "%s %lu %d\n", __func__, pwr, cdev->pwr_state);
	if (pwr == cdev->pwr_state)	/* power state no change */
		goto dev_power_end;

	switch (pwr) {
	case NVC_PWR_OFF:
	case NVC_PWR_STDBY_OFF:
		if (chip->power_off)
			err |= chip->power_off(cdev);
		camera_edp_lowest(cdev);
		break;
	case NVC_PWR_STDBY:
	case NVC_PWR_COMM:
	case NVC_PWR_ON:
		if (chip->power_on)
			err = chip->power_on(cdev);
		break;
	default:
		err = -EINVAL;
		break;
	}

	if (err < 0) {
		dev_err(cam->dev, "%s error\n", __func__);
		pwr = NVC_PWR_ERR;
	}

	cdev->pwr_state = pwr;
	if (err > 0)
		err = 0;

dev_power_end:
	return err;
}

static int camera_dev_pwr_get(struct camera_info *cam, unsigned long arg)
{
	int pwr;
	int err = 0;

	pwr = cam->cdev->pwr_state;
	dev_dbg(cam->dev, "%s PWR_RD: %d\n", __func__, pwr);
	if (copy_to_user((void __user *)arg, (const void *)&pwr,
		sizeof(pwr))) {
		dev_err(cam->dev, "%s copy_to_user err line %d\n",
			__func__, __LINE__);
		err = -EFAULT;
	}

	return err;
}

static struct camera_chip *camera_chip_chk(char *name)
{
	struct camera_chip *ccp;

	mutex_lock(cam_desc.c_mutex);
	list_for_each_entry(ccp, cam_desc.chip_list, list)
		if (!strcmp(ccp->name, name)) {
			dev_dbg(cam_desc.dev, "%s device %s found.\n",
				__func__, name);
			mutex_unlock(cam_desc.c_mutex);
			return ccp;
		}
	mutex_unlock(cam_desc.c_mutex);
	return NULL;
}

int camera_chip_add(struct camera_chip *chip)
{
	struct camera_chip *ccp;
	int err = 0;

	dev_dbg(cam_desc.dev, "%s add chip: %s\n", __func__, chip->name);
	mutex_lock(cam_desc.c_mutex);
	list_for_each_entry(ccp, cam_desc.chip_list, list)
		if (!strcmp(ccp->name, chip->name)) {
			dev_notice(cam_desc.dev, "%s chip %s already added.\n",
				__func__, chip->name);
			mutex_unlock(cam_desc.c_mutex);
			return -EEXIST;
		}

	list_add_tail(&chip->list, cam_desc.chip_list);
	mutex_unlock(cam_desc.c_mutex);
	return err;
}
EXPORT_SYMBOL_GPL(camera_chip_add);

static int camera_remove_device(struct camera_device *cdev, bool ref_dec)
{
	dev_dbg(cdev->dev, "%s %s\n", __func__, cdev->name);
	if (!cdev) {
		dev_err(cam_desc.dev, "%s cdev is NULL!\n", __func__);
		return -EFAULT;
	}

	WARN_ON(atomic_xchg(&cdev->in_use, 0));
	if (cdev->cam) {
		struct camera_info *cam = cdev->cam;
		cam->cdev = NULL;
		cam->dev = cam_desc.dev;
		atomic_set(&cam->in_use, 0);
	}
	if (cdev->chip)
		(cdev->chip->release)(cdev);
	if (cdev->dev)
		i2c_unregister_device(to_i2c_client(cdev->dev));
	if (ref_dec)
		atomic_dec(&cdev->chip->ref_cnt);
	kfree(cdev);
	return 0;
}

static int camera_free_device(struct camera_info *cam, unsigned long arg)
{
	struct camera_device *cdev = cam->cdev;

	dev_dbg(cam->dev, "%s %lx", __func__, arg);
	mutex_lock(cam_desc.d_mutex);
	atomic_set(&cdev->in_use, 0);
	mutex_unlock(cam_desc.d_mutex);

	return 0;
}

static int camera_new_device(struct camera_info *cam, unsigned long arg)
{
	struct camera_device_info dev_info;
	struct camera_device *new_dev;
	struct camera_device *next_dev;
	struct camera_chip *c_chip;
	struct i2c_adapter *adap;
	struct i2c_board_info brd;
	struct i2c_client *client;
	int err = 0;

	dev_dbg(cam->dev, "%s %lx", __func__, arg);
	if (copy_from_user(
		&dev_info, (const void __user *)arg, sizeof(dev_info))) {
		dev_err(cam_desc.dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
			err = -EFAULT;
			goto new_device_end;
	}
	dev_dbg(cam->dev, "%s - %d %d %x\n",
		dev_info.name, dev_info.type, dev_info.bus, dev_info.addr);

	c_chip = camera_chip_chk(dev_info.name);
	if (c_chip == NULL) {
		dev_err(cam->dev, "%s device %s not found\n",
			__func__, dev_info.name);
		err = -ENODEV;
		goto new_device_end;
	}

	adap = i2c_get_adapter(dev_info.bus);
	if (!adap) {
		dev_err(cam_desc.dev, "%s no such i2c bus %d\n",
			__func__, dev_info.bus);
		err = -ENODEV;
		goto new_device_end;
	}

	new_dev = kzalloc(sizeof(*new_dev), GFP_KERNEL);
	if (!new_dev) {
		dev_err(cam_desc.dev, "%s memory low!\n", __func__);
		err = -ENOMEM;
		goto new_device_end;
	}

	new_dev->chip = c_chip;
	memset(&brd, 0, sizeof(brd));
	strncpy(brd.type, dev_info.name, sizeof(brd.type));
	brd.addr = dev_info.addr;

	mutex_lock(cam_desc.d_mutex);

	/* check if device exists, if yes and not occupied pick it and exit */
	list_for_each_entry(next_dev, cam_desc.dev_list, list) {
		if (next_dev->client &&
			next_dev->client->adapter == adap &&
			next_dev->client->addr == dev_info.addr) {
			dev_dbg(cam_desc.dev,
				"%s: device already exists.\n", __func__);
			camera_remove_device(new_dev, false);
			if (atomic_xchg(&next_dev->in_use, 1)) {
				dev_err(cam_desc.dev, "%s device %s BUSY\n",
					__func__, next_dev->name);
				err = -EBUSY;
				goto new_device_err;
			}
			new_dev = next_dev;
			goto new_device_done;
		}
	}

	/* device is not present in the dev_list, add it */
	client = i2c_new_device(adap, &brd);
	if (!client) {
		dev_err(cam_desc.dev,
			"%s cannot allocate client: %s bus %d, %x\n",
			__func__, dev_info.name, dev_info.bus, dev_info.addr);
		err = -EINVAL;
		goto new_device_err;
	}
	new_dev->dev = &client->dev;

	new_dev->regmap = devm_regmap_init_i2c(client, &c_chip->regmap_cfg);
	if (IS_ERR(new_dev->regmap)) {
		dev_err(new_dev->dev, "%s regmap init failed: %ld\n",
			__func__, PTR_ERR(new_dev->regmap));
		err = -ENODEV;
		goto new_device_err;
	}
	strncpy(new_dev->name, dev_info.name, sizeof(new_dev->name));
	INIT_LIST_HEAD(&new_dev->list);
	mutex_init(&new_dev->mutex);
	new_dev->client = client;

	err = (new_dev->chip->init)(new_dev, cam_desc.pdata);
	if (err)
		goto new_device_err;

	atomic_inc(&c_chip->ref_cnt);
	atomic_set(&new_dev->in_use, 1);

	list_add(&new_dev->list, cam_desc.dev_list);

new_device_done:
	mutex_unlock(cam_desc.d_mutex);

	cam->cdev = new_dev;
	cam->dev = new_dev->dev;
	new_dev->cam = cam;
	goto new_device_end;

new_device_err:
	mutex_unlock(cam_desc.d_mutex);
	camera_remove_device(new_dev, false);

new_device_end:
	return err;
}

static void camera_app_remove(struct camera_info *cam)
{
	dev_dbg(cam->dev, "%s\n", __func__);

	WARN_ON(atomic_xchg(&cam->in_use, 0));
	if (cam->cdev) {
		WARN_ON(atomic_xchg(&cam->cdev->in_use, 0));
		cam->cdev->cam = NULL;
	}
	kfree(cam);
}

static int camera_update(struct camera_info *cam, unsigned long arg)
{
	struct camera_device *cdev = cam->cdev;
	struct camera_chip *chip = cdev->chip;
	struct nvc_param param;
	struct cam_update *upd = NULL;
	int err = 0;

	dev_dbg(cam->dev, "%s %lx", __func__, arg);
	if (!chip->update) {
		dev_dbg(cam->dev, "no update pointer.\n");
		goto update_end;
	}

	if (copy_from_user(&param, (const void __user *)arg, sizeof(param))) {
		dev_err(cam->dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
		err = -EFAULT;
		goto update_end;
	}
	upd = kzalloc(param.sizeofvalue * sizeof(*upd), GFP_KERNEL);
	if (!upd) {
		dev_err(cam->dev, "%s allocate memory failed!\n", __func__);
		err = -ENOMEM;
		goto update_end;
	}
	if (copy_from_user(upd, (const void __user *)param.p_value,
		param.sizeofvalue * sizeof(*upd))) {
		dev_err(cam->dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
		err = -EFAULT;
		goto update_end;
	}

	err = chip->update(cdev, upd, param.sizeofvalue);

update_end:
	kfree(upd);
	return err;
}

static int camera_layout_update(struct camera_info *cam, unsigned long arg)
{
	struct nvc_param param;
	void *upd = NULL;
	int err = 0;

	dev_dbg(cam->dev, "%s %lx", __func__, arg);
	mutex_lock(cam_desc.u_mutex);
	if (cam_desc.layout) {
		dev_notice(cam->dev, "layout already there.\n");
		err = -EEXIST;
		goto layout_end;
	}

	if (copy_from_user(&param, (const void __user *)arg, sizeof(param))) {
		dev_err(cam->dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
		err = -EFAULT;
		goto layout_end;
	}

	upd = kzalloc(param.sizeofvalue, GFP_KERNEL);
	if (!upd) {
		dev_err(cam->dev, "%s allocate memory failed!\n", __func__);
		err = -ENOMEM;
		goto layout_end;
	}
	if (copy_from_user(upd, (const void __user *)param.p_value,
		param.sizeofvalue)) {
		dev_err(cam->dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
		kfree(upd);
		err = -EFAULT;
		goto layout_end;
	}

	cam_desc.layout = upd;
	cam_desc.size_layout = param.sizeofvalue;

layout_end:
	mutex_unlock(cam_desc.u_mutex);
	return err;
}

static int camera_layout_get(struct camera_info *cam, unsigned long arg)
{
	struct nvc_param param;
	int len;
	int err = 0;

	dev_dbg(cam->dev, "%s %lx", __func__, arg);
	if (!cam_desc.layout) {
		dev_notice(cam->dev, "layout empty.\n");
		err = -EEXIST;
		goto getlayout_end;
	}

	if (copy_from_user(&param, (const void __user *)arg, sizeof(param))) {
		dev_err(cam->dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
		err = -EFAULT;
		goto getlayout_end;
	}

	len = (int)cam_desc.size_layout - param.variant;
	if (len > param.sizeofvalue) {
		len = param.sizeofvalue;
		err = -EAGAIN;
	}
	if (copy_to_user((void __user *)param.p_value,
		cam_desc.layout + param.variant, len)) {
		dev_err(cam->dev, "%s copy_to_user err line %d\n",
			__func__, __LINE__);
		err = -EFAULT;
		goto getlayout_end;
	}

	param.sizeofvalue = len;
	if (copy_to_user((void __user *)arg, &param, sizeof(param))) {
		dev_err(cam->dev, "%s copy_to_user err line %d\n",
			__func__, __LINE__);
		err = -EFAULT;
	}

getlayout_end:
	return err;
}

static int camera_add_drivers(struct camera_info *cam, unsigned long arg)
{
	struct nvc_param param;
	struct camera_module *cm = cam_desc.pdata->modules;
	struct i2c_adapter *adap = NULL;
	char ref_name[CAMERA_MAX_NAME_LENGTH];
	struct i2c_client *client = NULL;
	int err = 0;

	dev_dbg(cam->dev, "%s %lx", __func__, arg);
	if (copy_from_user(&param, (const void __user *)arg, sizeof(param))) {
		dev_err(cam->dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
		err = -EFAULT;
		goto add_driver_end;
	}

	if (param.sizeofvalue > sizeof(ref_name) - 1) {
		dev_err(cam->dev, "%s driver name too long %d\n",
			__func__, param.sizeofvalue);
		err = -EFAULT;
		goto add_driver_end;
	}

	memset(ref_name, 0, sizeof(ref_name));
	if (copy_from_user(ref_name, (const void __user *)param.p_value,
		param.sizeofvalue)) {
		dev_err(cam->dev, "%s copy_from_user err line %d\n",
			__func__, __LINE__);
		err = -EFAULT;
		goto add_driver_end;
	}

	dev_dbg(cam->dev, "%s looking for %s on %d\n",
		__func__, ref_name, param.variant);
	adap = i2c_get_adapter(param.variant);
	while (cm && cm->sensor && strlen(cm->sensor->type)) {
		dev_dbg(cam->dev, "%s\n", cm->sensor->type);
		if (!strcmp(cm->sensor->type, ref_name)) {
			dev_dbg(cam->dev, "installing %s\n", cm->sensor->type);
			client = i2c_new_device(adap, cm->sensor);
			if (!client) {
				dev_err(cam->dev, "%s add driver %s fail\n",
					__func__, cm->sensor->type);
				err = -EFAULT;
				break;
			}
			if (cm->focuser && strlen(cm->focuser->type)) {
				dev_dbg(cam->dev, "installing %s\n",
					cm->focuser->type);
				client = i2c_new_device(adap, cm->focuser);
				if (!client) {
					dev_err(cam->dev,
						"%s add driver %s fail\n",
						__func__, cm->focuser->type);
					err = -EFAULT;
					break;
				}
			}
			if (cm->flash && strlen(cm->flash->type)) {
				dev_dbg(cam->dev, "installing %s\n",
					cm->flash->type);
				client = i2c_new_device(adap, cm->flash);
				if (!client) {
					dev_err(cam->dev,
						"%s add driver %s fail\n",
						__func__, cm->flash->type);
					err = -EFAULT;
					break;
				}
			}
		}
		cm++;
	}

add_driver_end:
	return err;
}

static long camera_ioctl(struct file *file,
			 unsigned int cmd,
			 unsigned long arg)
{
	struct camera_info *cam = file->private_data;
	int err = 0;

	dev_dbg(cam->dev, "%s %x %lx\n", __func__, cmd, arg);
	if (!cam->cdev && ((cmd == PCLLK_IOCTL_SEQ_WR) ||
		(cmd == PCLLK_IOCTL_PWR_WR) ||
		(cmd == PCLLK_IOCTL_PWR_RD))) {
		dev_err(cam_desc.dev, "%s %x - no device activated.\n",
			__func__, cmd);
		err = -ENODEV;
		goto ioctl_end;
	}

	/* command distributor */
	switch (_IOC_NR(cmd)) {
	case _IOC_NR(PCLLK_IOCTL_CHIP_REG):
		err = virtual_device_add(cam_desc.dev, arg);
		break;
	case _IOC_NR(PCLLK_IOCTL_DEV_REG):
		err = camera_new_device(cam, arg);
		break;
	case _IOC_NR(PCLLK_IOCTL_DEV_DEL):
		mutex_lock(cam_desc.d_mutex);
		list_del(&cam->cdev->list);
		mutex_unlock(cam_desc.d_mutex);
		camera_remove_device(cam->cdev, true);
		break;
	case _IOC_NR(PCLLK_IOCTL_DEV_FREE):
		err = camera_free_device(cam, arg);
		break;
	case _IOC_NR(PCLLK_IOCTL_SEQ_WR):
		err = camera_seq_wr(cam, arg);
		break;
	case _IOC_NR(PCLLK_IOCTL_SEQ_RD):
		err = camera_seq_rd(cam, arg);
		break;
	case _IOC_NR(PCLLK_IOCTL_PARAM_RD):
		/* err = camera_param_rd(cam, arg); */
		break;
	case _IOC_NR(PCLLK_IOCTL_PWR_WR):
		/* This is a Guaranteed Level of Service (GLOS) call */
		err = camera_dev_pwr_set(cam, arg);
		break;
	case _IOC_NR(PCLLK_IOCTL_PWR_RD):
		err = camera_dev_pwr_get(cam, arg);
		break;
	case _IOC_NR(PCLLK_IOCTL_UPDATE):
		err = camera_update(cam, arg);
		break;
	case _IOC_NR(PCLLK_IOCTL_LAYOUT_WR):
		err = camera_layout_update(cam, arg);
		break;
	case _IOC_NR(PCLLK_IOCTL_LAYOUT_RD):
		err = camera_layout_get(cam, arg);
		break;
	case _IOC_NR(PCLLK_IOCTL_DRV_ADD):
		err = camera_add_drivers(cam, arg);
		break;
	default:
		dev_err(cam->dev, "%s unsupported ioctl: %x\n",
			__func__, cmd);
		err = -EINVAL;
	}

ioctl_end:
	if (err)
		dev_dbg(cam->dev, "err = %d\n", err);

	return err;
}

static int camera_open(struct inode *inode, struct file *file)
{
	struct camera_info *cam;

	cam = kzalloc(sizeof(*cam), GFP_KERNEL);
	if (!cam) {
		dev_err(cam_desc.dev,
			"%s unable to allocate memory!\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&cam->k_mutex);
	atomic_set(&cam->in_use, 0);
	INIT_LIST_HEAD(&cam->list);
	cam->dev = cam_desc.dev;
	file->private_data = cam;

	mutex_lock(cam_desc.u_mutex);
	list_add(&cam->list, cam_desc.app_list);
	mutex_unlock(cam_desc.u_mutex);

	dev_dbg(cam_desc.dev, "%s\n", __func__);
	return 0;
}

static int camera_release(struct inode *inode, struct file *file)
{
	struct camera_info *cam = file->private_data;

	dev_dbg(cam_desc.dev, "%s\n", __func__);

	mutex_lock(cam_desc.u_mutex);
	list_del(&cam->list);
	mutex_unlock(cam_desc.u_mutex);

	camera_app_remove(cam);

	file->private_data = NULL;
	return 0;
}

static const struct file_operations camera_fileops = {
	.owner = THIS_MODULE,
	.open = camera_open,
	.unlocked_ioctl = camera_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = camera_ioctl,
#endif
	.release = camera_release,
};

static int camera_remove(struct platform_device *dev)
{
	struct camera_info *cam;
	struct camera_device *cdev;

	dev_dbg(cam_desc.dev, "%s\n", __func__);
	misc_deregister(&cam_desc.miscdev);

	list_for_each_entry(cam, cam_desc.app_list, list) {
		mutex_lock(cam_desc.u_mutex);
		list_del(&cam->list);
		mutex_unlock(cam_desc.u_mutex);
		camera_app_remove(cam);
	}

	list_for_each_entry(cdev, cam_desc.dev_list, list) {
		mutex_lock(cam_desc.d_mutex);
		list_del(&cdev->list);
		mutex_unlock(cam_desc.d_mutex);
		camera_remove_device(cdev, true);
	}

	kfree(cam_desc.layout);

	camera_debugfs_remove();
	return 0;
}

static int camera_probe(struct platform_device *dev)
{
	dev_dbg(&dev->dev, "%s\n", __func__);

	if (atomic_xchg(&cam_desc.in_use, 1)) {
		dev_err(&dev->dev, "%s OCCUPIED!\n", __func__);
		return -EBUSY;
	}

	cam_desc.dev = &dev->dev;
	if (dev->dev.platform_data) {
		cam_desc.pdata = dev->dev.platform_data;
	} else {
		cam_desc.pdata = &camera_dflt_pdata;
		dev_dbg(cam_desc.dev, "%s No platform data.  Using defaults.\n",
			__func__);
	}
	dev_dbg(&dev->dev, "%x\n", cam_desc.pdata->cfg);

	strcpy(cam_desc.dname, "camera.pcl");
	dev_set_drvdata(&dev->dev, &cam_desc);

	cam_desc.miscdev.name = cam_desc.dname;
	cam_desc.miscdev.fops = &camera_fileops;
	cam_desc.miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&cam_desc.miscdev)) {
		dev_err(cam_desc.dev, "%s unable to register misc device %s\n",
			__func__, cam_desc.dname);
		return -ENODEV;
	}

	camera_debugfs_init(&cam_desc);
	return 0;
}

static const struct platform_device_id camera_id[] = {
	{ "pcl-generic", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, camera_id);

static struct platform_driver camera_driver = {
	.driver = {
		.name = "pcl-generic",
		.owner = THIS_MODULE,
	},
	.id_table = camera_id,
	.probe = camera_probe,
	.remove = camera_remove,
};

module_platform_driver(camera_driver);

MODULE_DESCRIPTION("Generic Camera Device Driver");
MODULE_AUTHOR("Charlie Huang <chahuang@nvidia.com>");
MODULE_LICENSE("GPL v2");
