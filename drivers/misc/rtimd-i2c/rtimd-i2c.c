/*
 * File name: rtimd-i2c.c
 *
 * Description : RAONTECH Micro Display I2C driver.
 *
 * Copyright (C) (2017, RAONTECH)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "rtimd-i2c.h"

#define SYSFS_BURST_DATA_BUF_SIZE		1024

#define SYSFS_BWR_DATA_OFFSET		11
#define SYSFS_BRD_WDATA_OFFSET		16

static struct platform_device *rtimd_device;
static struct class *rtimd_class;

struct RTIMD_CB_T *rtimd_cb;

static struct RTIMD_SINGLE_READ_REG_T srd_param;

static struct RTIMD_BURST_READ_REG_T brd_param;
static uint8_t sysfs_brd_wdata[SYSFS_BURST_DATA_BUF_SIZE];
static uint8_t sysfs_brd_rdata[SYSFS_BURST_DATA_BUF_SIZE];

static uint8_t sysfs_bwr_data[SYSFS_BURST_DATA_BUF_SIZE];

/* Forward functions */
static int rtimd_probe(struct platform_device *pdev);
static int rtimd_remove(struct platform_device *pdev);


static int change_i2c_bus(int new_bus_num)
{
	int rc = 0;
	/* Close the previous bus if opened. */
	if (rtimd_cb->adap)
		i2c_put_adapter(rtimd_cb->adap);

	rtimd_cb->adap = i2c_get_adapter(new_bus_num);
	if (rtimd_cb->adap) {
		rtimd_cb->bus_num = new_bus_num; /* Set new bus number */
		rc = 0;
	} else {
		rtimd_cb->bus_num = -1;
		RMDERR("I2C device not found.\n");
		rc = -ENODEV;
	}
	return rc;
}

static int i2c_burst_read(struct i2c_adapter *adap,
			struct RTIMD_BURST_READ_REG_T *br, uint8_t *wbuf, uint8_t *rbuf)
{
	int ret = 0;
	struct i2c_msg msgs[2] = {
		{
			.addr = br->slave_addr,
			.flags = 0,
			.buf = wbuf,
			.len = br->wsize
		},
		{
			.addr = br->slave_addr,
			.flags = I2C_M_RD,
			.buf = rbuf,
			.len = br->rsize
		}
	};

	pr_debug("bus(%d) slave_addr(0x%02X) msgs[0].buf[0](0x%02X)\n",
			br->bus_num, br->slave_addr, msgs[0].buf[0]);
	ret = i2c_transfer(adap, msgs, 2);

	/* If everything went ok, return #bytes transmitted, else error code. */
	return (ret == 2) ? br->rsize : ret;
}

static int i2c_burst_write(struct i2c_adapter *adap,
					struct RTIMD_BURST_WRITE_REG_T *bw, uint8_t *wbuf)
{
	int ret = 0;
	struct i2c_msg msgs = {
		.addr = bw->slave_addr,
		.flags = 0,
		.buf = wbuf,
		.len = bw->wsize
	};

	ret = i2c_transfer(adap, &msgs, 1);

	/*
	 * If everything went ok (i.e. 1 msg transmitted), return #bytes
	 * transmitted, else error code.
	 */
	return (ret == 1) ? msgs.len : ret;
}

static int i2c_single_write(struct i2c_adapter *adap, struct RTIMD_SINGLE_WRITE_REG_T *sw)
{
	uint8_t wbuf[3]; /* max reg size is 2. max data size is 1 */
	int ret = 0;
	struct i2c_msg msgs = {
		.addr = sw->slave_addr,
		.flags = 0,
		.buf = wbuf,
		.len = sw->reg_size + 1/*data*/
	};

	switch (sw->reg_size) {
	case 1:
		wbuf[0] = sw->reg_addr & 0xFF;
		wbuf[1] = sw->data;
		break;

	case 2:
		wbuf[0] = sw->reg_addr >> 8;
		wbuf[1] = sw->reg_addr & 0xFF;
		wbuf[2] = sw->data;
		break;

	default:
		RMDERR("Invalid register size\n");
		return -EINVAL;
	}

	pr_debug("sw: bus_num(%d) saddr(0x%02X) regaddr(0x%04X) data(0x%02X)\n",
		sw->bus_num, sw->slave_addr, sw->reg_addr, sw->data);

	ret = i2c_transfer(adap, &msgs, 1);

	/*
	 * If everything went ok (i.e. 1 msg transmitted), return #bytes
	 * transmitted, else error code.
	 */
	return (ret == 1) ? msgs.len : ret;
}

static inline int i2c_single_read(struct i2c_adapter *adap,
					struct RTIMD_SINGLE_READ_REG_T *sr, uint8_t *rbuf)
{
	uint8_t wbuf[2]; /* max reg size is 2. */
	int ret = 0;
	struct i2c_msg msgs[2] = {
		{
			.addr = sr->slave_addr,
			.flags = 0,
			.buf = wbuf,
			.len = sr->reg_size
		},
		{
			.addr = sr->slave_addr,
			.flags = I2C_M_RD,
			.buf = rbuf,
			.len = 1
		}
	};

	switch (sr->reg_size) {
	case 1:
		wbuf[0] = sr->reg_addr & 0xFF;
		break;

	case 2:
		wbuf[0] = sr->reg_addr >> 8;
		wbuf[1] = sr->reg_addr & 0xFF;
		break;

	default:
		RMDERR("Invalid register size\n");
		return -EINVAL;
	}

	pr_debug("adap(0x%p) bus_num(%d) addr(0x%02X) reg_size(%u) wbuf[0](0x%02X)\n",
		rtimd_cb->adap, sr->bus_num, msgs[0].addr, sr->reg_size, wbuf[0]);

	ret = i2c_transfer(adap, msgs, 2);

	/* If everything went ok, return #bytes transmitted, else error code. */
	if (ret != 2) {
		RMDERR("i2c 0x%0X read failed! ret(%d)\n", sr->reg_addr, ret);
		return ret;
	}

	return msgs[1].len;
}

static inline int ioctl_burst_write(unsigned long arg)
{
	int ret;
	struct RTIMD_BURST_WRITE_REG_T bw;
	struct RTIMD_BURST_WRITE_REG_T __user *argp = (struct RTIMD_BURST_WRITE_REG_T __user *)arg;

	if (copy_from_user(&bw, argp, sizeof(struct RTIMD_BURST_WRITE_REG_T))) {
		RMDERR("copy_from_user() failed.\n");
		return -EFAULT;
	}

	if (rtimd_cb->bus_num != bw.bus_num) {
		ret = change_i2c_bus(bw.bus_num);
		if (ret != 0)
			return ret;
	}

	if (copy_from_user(rtimd_cb->write_buf, (u8 __user *)bw.wbuf_addr, bw.wsize)) {
		RMDERR("copy_from_user() failed.\n");
		return -EFAULT;
	}

	return i2c_burst_write(rtimd_cb->adap, &bw, rtimd_cb->write_buf);
}

static inline int ioctl_single_write(unsigned long arg)
{
	int ret;
	struct RTIMD_SINGLE_WRITE_REG_T sw;
	struct RTIMD_SINGLE_WRITE_REG_T __user *argp =
			(struct RTIMD_SINGLE_WRITE_REG_T __user *)arg;

	if (copy_from_user(&sw, argp, sizeof(struct RTIMD_SINGLE_WRITE_REG_T))) {
		RMDERR("copy_from_user() failed.\n");
		return -EFAULT;
	}

	if (rtimd_cb->bus_num != sw.bus_num) {
		ret = change_i2c_bus(sw.bus_num);
		if (ret != 0)
			return ret;
	}

	return i2c_single_write(rtimd_cb->adap, &sw);
}

static inline int ioctl_burst_read(unsigned long arg)
{
	int ret;
	struct RTIMD_BURST_READ_REG_T br;
	struct RTIMD_BURST_READ_REG_T __user *argp = (struct RTIMD_BURST_READ_REG_T __user *)arg;

	if (copy_from_user(&br, argp, sizeof(struct RTIMD_BURST_READ_REG_T))) {
		RMDERR("copy_from_user() failed.\n");
		return -EFAULT;
	}

	if (br.rsize > MAX_RTIMD_REG_DATA_SIZE) {
		RMDERR("Invalid count to be read register\n");
		return -EINVAL;
	}

	if (copy_from_user(rtimd_cb->write_buf, (u8 __user *)br.wbuf_addr, br.wsize)) {
		RMDERR("copy_from_user() failed.\n");
		return -EFAULT;
	}

	if (rtimd_cb->bus_num != br.bus_num) {
		ret = change_i2c_bus(br.bus_num);
		if (ret != 0)
			return ret;
	}

	ret = i2c_burst_read(rtimd_cb->adap, &br, rtimd_cb->write_buf,
						rtimd_cb->read_buf);
	if (ret > 0) {
		ret = copy_to_user((u8 __user *)br.rbuf_addr,
				rtimd_cb->read_buf, br.rsize) ? -EFAULT : ret;
	}

	return ret;
}

static int ioctl_single_read(unsigned long arg)
{
	uint8_t sbuf; /* Single reade buffer */
	int ret;
	struct RTIMD_SINGLE_READ_REG_T sr;
	struct RTIMD_SINGLE_READ_REG_T __user *argp = (struct RTIMD_SINGLE_READ_REG_T __user *)arg;

	if (copy_from_user(&sr, argp, sizeof(struct RTIMD_SINGLE_READ_REG_T))) {
		RMDERR("copy_from_user() failed.\n");
		return -EFAULT;
	}

	if (rtimd_cb->bus_num != sr.bus_num) {
		ret = change_i2c_bus(sr.bus_num);
		if (ret != 0)
			return ret;
	}

	ret = i2c_single_read(rtimd_cb->adap, &sr, &sbuf);
	if (ret > 0) {
		if (put_user(sbuf, (u8 __user *)sr.rbuf_addr)) {
			RMDERR("put_user() failed.\n");
			ret = -EFAULT;
		}
	}

	return ret;
}

static long rtimd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;

	switch (cmd) {
	case IOCTL_RTIMD_SINGLE_READ:
		ret = ioctl_single_read(arg);
		break;

	case IOCTL_RTIMD_BURST_READ:
		ret = ioctl_burst_read(arg);
		break;

	case IOCTL_RTIMD_SINGLE_WRITE:
		ret = ioctl_single_write(arg);
		break;

	case IOCTL_RTIMD_BURST_WRITE:
		ret = ioctl_burst_write(arg);
		break;

	default:
		RMDERR("Invalid ioctl command\n");
		ret = ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long compat_rtimd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return rtimd_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

/*
 * If successful, function returns the number of bytes actually written.
 * NOTE: For the single write mode, count vlaue don't care!
 */
static ssize_t rtimd_write(struct file *file, const char __user *buf,
							size_t count, loff_t *offset)
{
	RMDERR("Unsupport!\n");
	return 0;
}

static ssize_t rtimd_read(struct file *file, char __user *buf, size_t count,
						loff_t *offset)
{
	RMDERR("Unsupport!\n");
	return 0;
}

static int rtimd_release(struct inode *inode, struct file *file)
{
	kfree(rtimd_cb->read_buf);
	rtimd_cb->read_buf = NULL;

	kfree(rtimd_cb->write_buf);
	rtimd_cb->write_buf = NULL;

	if (rtimd_cb->adap) {
		rtimd_cb->bus_num = -1; /* Set default bus number as invalid */
		i2c_put_adapter(rtimd_cb->adap);
	}

	atomic_set(&rtimd_cb->open_flag, 0);

	RMDDBG("Device closed\n");

	return 0;
}

static int rtimd_open(struct inode *inode, struct file *file)
{
	/* Check if the device is already opened ? */
	if (atomic_cmpxchg(&rtimd_cb->open_flag, 0, 1)) {
		RMDERR("%s is already opened\n", RTI_MD_DEV_NAME);
		return -EBUSY;
	}

	rtimd_cb->read_buf = kmalloc(MAX_RTIMD_REG_DATA_SIZE, GFP_KERNEL);
	if (!rtimd_cb->read_buf) {
		RMDERR("Fail to allocate a read buffer\n");
		return -ENOMEM;
	}

	rtimd_cb->write_buf = kmalloc(MAX_RTIMD_REG_DATA_SIZE, GFP_KERNEL);
	if (!rtimd_cb->write_buf) {
		RMDERR("Fail to allocate a write buffer\n");
		kfree(rtimd_cb->read_buf);
		rtimd_cb->read_buf = NULL;
		return -ENOMEM;
	}

	rtimd_cb->bus_num = -1; /* Set default bus number as invalid */
	rtimd_cb->adap = NULL;

	RMDDBG("Device opened\n");

	return 0;
}

static int rtimd_pm_suspend(struct device *dev)
{
	RMDDBG("\n");

	return 0;
}

static int rtimd_pm_resume(struct device *dev)
{
	RMDDBG("\n");

	return 0;
}

static const struct file_operations rtimd_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.read		= rtimd_read,
	.write		= rtimd_write,
	.unlocked_ioctl	= rtimd_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_rtimd_ioctl, /* 32-bit entry */
#endif
	.open		= rtimd_open,
	.release	= rtimd_release,
};

static const struct dev_pm_ops rtimd_dev_pm_ops = {
		.suspend = rtimd_pm_suspend,
		.resume = rtimd_pm_resume,
};

static struct platform_driver rtimd_driver = {
		.probe = rtimd_probe,
		.remove = __exit_p(rtimd_remove),
		.driver = {
				.name = RTI_MD_DEV_NAME,
				.pm = &rtimd_dev_pm_ops,
		}
};

static void hex_string_to_digit(uint8_t *out, const char *in, int len)
{
	int i, t;
	uint8_t hn, ln;
	char msb_ch, lsb_ch;

	for (t = 0, i = 0; i < len; i += 2, t++) {
		msb_ch = toupper(in[i]);
		lsb_ch = toupper(in[i + 1]);

		hn = (msb_ch > '9') ? (msb_ch - 'A' + 10) : (msb_ch - '0');
		ln = (lsb_ch > '9') ? (lsb_ch - 'A' + 10) : (lsb_ch - '0');

		out[t] = ((hn&0xF) << 4) | ln;
	}
}

/**
 * Set parameters to read a byte from register.
 *
 * ex) echo 06 44 005E 01 > /sys/devices/platform/rtimd-i2c/rtimd_srd_param
 *
 * Parameters: BNR SA RA RS
 * BNR: Bus Number (2 hex)
 * SA: Slave Address (2 hex)
 * RA: Register address of RDC or RDP (4 hex)
 * RS: Register size of RDC or RDP (2 hex)
 */
static ssize_t rtimd_srd_param_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	unsigned int bus_num, slave_addr, reg_addr, reg_size;
	struct RTIMD_SINGLE_READ_REG_T *param = &srd_param;

	rc = sscanf(buf, "%X %X %X %X", &bus_num, &slave_addr, &reg_addr, &reg_size);

	param->bus_num = (uint8_t)bus_num;
	param->slave_addr = (uint8_t)slave_addr;
	param->reg_addr = (uint32_t)reg_addr;
	param->reg_size = (uint8_t)reg_size;

	return count;
}

/**
 * Get a byte from register using the saved parameters
 * in rtimd_sreg_show().
 *
 * ex) cat /sys/devices/platform/rtimd-i2c/rtimd_sreg
 */
static ssize_t rtimd_sreg_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int ret;
	uint8_t sbuf; /* Single reade buffer */
	struct i2c_adapter *adap;
	ssize_t count;
	struct RTIMD_SINGLE_READ_REG_T *param = &srd_param;

	adap = i2c_get_adapter(param->bus_num);
	if (adap == NULL) {
		RMDERR("I2C adapter open failed.\n");
		return -ENODEV;
	}

	ret = i2c_single_read(adap, param, &sbuf);
	if (ret > 0)
		count = scnprintf(buf, sizeof("00 %02X\n"), "00 %02X\n", sbuf);
	else
		count = scnprintf(buf, sizeof("FF\n"), "FF\n");

	i2c_put_adapter(adap);

	return count;
}

/**
 * Write a byte to register.
 *
 * ex) echo 06 44 005E 01 0x7A > /sys/devices/platform/rtimd-i2c/rtimd_sreg
 * Parameters: BNR SA RA RS VAL
 * BNR: Bus Number (2 hex)
 * SA: Slave Address (2 hex)
 * RA: Register address of RDC or RDP (4 hex)
 * RS: Register size of RDC or RDP (2 hex)
 * VAL: Register value to be written (2 hex)
 */
static ssize_t rtimd_sreg_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	struct i2c_adapter *adap;
	unsigned int bus_num, slave_addr, reg_addr, reg_size, data;
	static struct RTIMD_SINGLE_WRITE_REG_T swr_param;

	rc = sscanf(buf, "%X %X %X %X %X", &bus_num, &slave_addr, &reg_addr, &reg_size, &data);

	swr_param.bus_num = (uint8_t)bus_num;
	swr_param.slave_addr = (uint8_t)slave_addr;
	swr_param.reg_addr = (uint8_t)reg_addr;
	swr_param.reg_size = (uint8_t)reg_size;
	swr_param.data = (uint8_t)data;

	adap = i2c_get_adapter(swr_param.bus_num);
	if (adap == NULL) {
		RMDERR("I2C adapter open failed.\n");
		return -ENODEV;
	}

	i2c_single_write(adap, &swr_param);

	i2c_put_adapter(adap);

	return count;
}

/**
 * Set parameters to read the multiple bytes from register.
 *
 * ex) echo 06 44 0001 0005 5E > /sys/devices/platform/rtimd-i2c/rtimd_brd_param
 * Parameters: BNR SA WSIZE RSIZE WDATA
 *	BNR: Bus Number (2 hex)
 *	SA: Slave Address (2 hex)
 *	WSIZE: Number of bytes to write to the device before READ command in
 *		   I2C protocol. (4 hex)
 * RSIZE: Number of bytes to be read from the device (4 hex)
 * WDATA: Data to write to the device before READ command in I2C protocol.
 */
static ssize_t rtimd_brd_param_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	unsigned int bus_num, slave_addr, wsize, rsize;
	const char *buf_ptr = buf;
	struct RTIMD_BURST_READ_REG_T *param = &brd_param;

	rc = sscanf(buf, "%X %X %X %X", &bus_num, &slave_addr, &wsize, &rsize);

	buf_ptr += SYSFS_BRD_WDATA_OFFSET;

	if (wsize > 512) {
		RMDERR("Exceed the number of write bytes.\n");
		return -EINVAL;
	}

	if (rsize > 512) {
		RMDERR("Exceed the number of read bytes.\n");
		return -EINVAL;
	}

	/* 1hex: 2bytes asscii */
	hex_string_to_digit(sysfs_brd_wdata, buf_ptr, wsize<<1);

	param->bus_num = (uint8_t)bus_num;
	param->slave_addr = (uint8_t)slave_addr;
	param->wsize = (uint16_t)wsize;
	param->rsize = (uint16_t)rsize;

	return count;
}

/**
 * Get the multiple bytes from register using the saved parameters
 * in store_rtimd_brd_set_param().
 *
 * ex) cat /sys/devices/platform/rtimd-i2c/rtimd_breg
 */
static ssize_t rtimd_breg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret, i;
	struct i2c_adapter *adap;
	ssize_t count;
	char nibble, hex_ch, *buf_ptr = buf;
	struct RTIMD_BURST_READ_REG_T *param = &brd_param;

	adap = i2c_get_adapter(param->bus_num);
	if (adap == NULL) {
		RMDERR("I2C adapter open failed.\n");
		return -ENODEV;
	}

	ret = i2c_burst_read(adap, param, sysfs_brd_wdata, sysfs_brd_rdata);
	if (ret > 0) {
		*buf_ptr++ = '0'; /* Success */
		*buf_ptr++ = '0';
		*buf_ptr++ = ' ';

		/* rdata */
		for (i = 0; i < param->rsize; i++) {
			nibble = (sysfs_brd_rdata[i] & 0xF0) >> 4;
			hex_ch = (nibble > 9) ? (nibble - 0xA + 'A') : (nibble + '0');
			*buf_ptr++ = hex_ch;

			nibble = sysfs_brd_rdata[i] & 0x0F;
			hex_ch = (nibble > 9) ? (nibble - 0xA + 'A') : (nibble + '0');
			*buf_ptr++ = hex_ch;
		}

		*buf_ptr++ = '\n';

		/*
		 * Returns the number of bytes stored in buffer,
		 * not counting the terminating null character.
		 */
		*buf_ptr = '\0';
		count = (ssize_t)(buf_ptr - buf);
	} else
		count = scnprintf(buf, sizeof("FF\n"), "FF\n");

	i2c_put_adapter(adap);

	return count;
}

/**
 * Write the multiple bytes to register.
 *
 * ex) echo 06 44 0006 5E123456789A > /sys/devices/platform/rtimd-i2c/rtimd_breg
 * Parameters: BNR SA WSIZE WDATA
 *	BNR: Bus Number
 *	SA: Slave Address
 *	WSIZE: Number of bytes to write to the device (4 ..)
 *	WDATA: Data to be written
 */
static ssize_t rtimd_breg_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_adapter *adap;
	int bus_num, rc = 0;
	unsigned int slave_addr, wsize;
	struct RTIMD_BURST_WRITE_REG_T bwr_param;

	rc = sscanf(buf, "%X %X %X", &bus_num, &slave_addr, &wsize);
	bwr_param.bus_num = (uint8_t)bus_num;
	bwr_param.slave_addr = (uint8_t)slave_addr;
	bwr_param.wsize = (uint16_t)wsize;

	if (wsize > 512) {
		RMDERR("Exceed the write bytes.\n");
		return -EINVAL;
	}

	/* 1hex: 2bytes asscii */
	hex_string_to_digit(sysfs_bwr_data, &buf[SYSFS_BWR_DATA_OFFSET], wsize<<1);

	adap = i2c_get_adapter(bus_num);
	if (adap == NULL) {
		RMDERR("I2C adapter open failed.\n");
		return -ENODEV;
	}

	i2c_burst_write(adap, &bwr_param, sysfs_bwr_data);

	i2c_put_adapter(adap);

	return count;
}

static ssize_t rtimd_eye_show(struct kobject *kobj, struct kobj_attribute *attr,
	char *buf, u16 reg1, u16 reg2, uint8_t slave_addr)
{
	int ret;
	uint8_t sbuf1, sbuf2; /* Single reade buffer */
	ssize_t count;
	struct RTIMD_SINGLE_READ_REG_T sw;

	rtimd_cb->adap = i2c_get_adapter(0);
	if (rtimd_cb->adap == NULL) {
		RMDERR("I2C adapter open failed.\n");
		return -ENODEV;
	}

	sw.reg_size = 2;
	sw.reg_addr = reg1;
	sw.bus_num = 0;
	sw.slave_addr = slave_addr;

	ret = i2c_single_read(rtimd_cb->adap, &sw, &sbuf1);

	sw.reg_size = 2;
	sw.reg_addr = reg2;
	sw.bus_num = 0;
	sw.slave_addr = slave_addr;

	ret = i2c_single_read(rtimd_cb->adap, &sw, &sbuf2);
	if (ret > 0)
		count = scnprintf(buf, sizeof("%02X %02X\n"), "%02X %02X\n", sbuf1, sbuf2);
	else
		count = scnprintf(buf, sizeof("FF\n"), "FF\n");

	i2c_put_adapter(rtimd_cb->adap);

	return count;
}

static ssize_t rtimd_eye_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count, uint16_t shift, u16 reg1, u16 reg2,
		uint8_t slave_addr, uint16_t data)
{
	int err = 0;
	unsigned long value;
	struct RTIMD_SINGLE_WRITE_REG_T sw;

	err = kstrtoul(buf, 10, &value);
	if (err != 0)
		return err;

	rtimd_cb->adap = i2c_get_adapter(0);

	sw.reg_size = 2;
	sw.reg_addr = reg1;
	sw.bus_num = 0;
	sw.slave_addr = slave_addr;
	sw.data = (value & data) >> 8;
	sw.data = sw.data | shift;

	err = i2c_single_write(rtimd_cb->adap, &sw);

	sw.reg_size = 2;
	sw.reg_addr = reg2;
	sw.bus_num = 0;
	sw.slave_addr = slave_addr;
	sw.data = value & 0xff;

	err = i2c_single_write(rtimd_cb->adap, &sw);

	i2c_put_adapter(rtimd_cb->adap);
	return count;
}

static ssize_t left_eye_shift_left_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u16 reg1 = 0x0830;
	u16 reg2 = 0x0831;
	uint8_t slave_addr = 0x4A;

	size_t ret = rtimd_eye_show(kobj, attr, buf, reg1, reg2, slave_addr);
	return ret;
}

static ssize_t left_eye_shift_left_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	u16 reg1 = 0x0830;
	u16 reg2 = 0x0831;
	uint8_t slave_addr = 0x4A;
	uint16_t data = 0xf00;
	uint16_t shift = 0x0;

	size_t ret = rtimd_eye_store(kobj, attr, buf, count,
		shift, reg1, reg2, slave_addr, data);
	return ret;
}

static ssize_t left_eye_shift_right_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u16 reg1 = 0x0830;
	u16 reg2 = 0x0831;
	uint8_t slave_addr = 0x4A;

	size_t ret = rtimd_eye_show(kobj, attr, buf, reg1, reg2, slave_addr);
	return ret;
}

static ssize_t left_eye_shift_right_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	u16 reg1 = 0x0830;
	u16 reg2 = 0x0831;
	uint8_t slave_addr = 0x4A;
	uint16_t data = 0xf00;
	uint16_t shift = 0x80;

	size_t ret = rtimd_eye_store(kobj, attr, buf, count,
		shift, reg1, reg2, slave_addr, data);
	return ret;
}

static ssize_t left_eye_shift_top_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u16 reg1 = 0x0832;
	u16 reg2 = 0x0833;
	uint8_t slave_addr = 0x4A;

	size_t ret = rtimd_eye_show(kobj, attr, buf, reg1, reg2, slave_addr);
	return ret;
}

static ssize_t left_eye_shift_top_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	u16 reg1 = 0x0832;
	u16 reg2 = 0x0833;
	uint8_t slave_addr = 0x4A;
	uint16_t data = 0x1f00;
	uint16_t shift = 0x0;

	size_t ret = rtimd_eye_store(kobj, attr, buf, count,
		shift, reg1, reg2, slave_addr, data);
	return ret;
}

static ssize_t left_eye_shift_bottom_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u16 reg1 = 0x0832;
	u16 reg2 = 0x0833;
	uint8_t slave_addr = 0x4A;

	size_t ret = rtimd_eye_show(kobj, attr, buf, reg1, reg2, slave_addr);
	return ret;
}

static ssize_t left_eye_shift_bottom_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	u16 reg1 = 0x0832;
	u16 reg2 = 0x0833;
	uint8_t slave_addr = 0x4A;
	uint16_t data = 0x1f00;
	uint16_t shift = 0x80;

	size_t ret = rtimd_eye_store(kobj, attr, buf, count,
		shift, reg1, reg2, slave_addr, data);
	return ret;
}

static ssize_t right_eye_shift_left_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u16 reg1 = 0x0830;
	u16 reg2 = 0x0831;
	uint8_t slave_addr = 0x4C;

	size_t ret = rtimd_eye_show(kobj, attr, buf, reg1, reg2, slave_addr);
	return ret;
}

static ssize_t right_eye_shift_left_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	u16 reg1 = 0x0830;
	u16 reg2 = 0x0831;
	uint8_t slave_addr = 0x4C;
	uint16_t data = 0xf00;
	uint16_t shift = 0x0;

	size_t ret = rtimd_eye_store(kobj, attr, buf, count,
		shift, reg1, reg2, slave_addr, data);
	return ret;
}

static ssize_t right_eye_shift_right_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u16 reg1 = 0x0830;
	u16 reg2 = 0x0831;
	uint8_t slave_addr = 0x4C;

	size_t ret = rtimd_eye_show(kobj, attr, buf, reg1, reg2, slave_addr);
	return ret;
}

static ssize_t right_eye_shift_right_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	u16 reg1 = 0x0830;
	u16 reg2 = 0x0831;
	uint8_t slave_addr = 0x4C;
	uint16_t data = 0xf00;
	uint16_t shift = 0x80;

	size_t ret = rtimd_eye_store(kobj, attr, buf, count,
		shift, reg1, reg2, slave_addr, data);
	return ret;
}

static ssize_t right_eye_shift_top_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u16 reg1 = 0x0832;
	u16 reg2 = 0x0833;
	uint8_t slave_addr = 0x4C;

	size_t ret = rtimd_eye_show(kobj, attr, buf, reg1, reg2, slave_addr);
	return ret;
}

static ssize_t right_eye_shift_top_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	u16 reg1 = 0x0832;
	u16 reg2 = 0x0833;
	uint8_t slave_addr = 0x4C;
	uint16_t data = 0x1f00;
	uint16_t shift = 0x0;

	size_t ret = rtimd_eye_store(kobj, attr, buf, count,
		shift, reg1, reg2, slave_addr, data);
	return ret;
}

static ssize_t right_eye_shift_bottom_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u16 reg1 = 0x0832;
	u16 reg2 = 0x0833;
	uint8_t slave_addr = 0x4C;

	size_t ret = rtimd_eye_show(kobj, attr, buf, reg1, reg2, slave_addr);
	return ret;
}

static ssize_t right_eye_shift_bottom_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	u16 reg1 = 0x0832;
	u16 reg2 = 0x0833;
	uint8_t slave_addr = 0x4C;
	uint16_t data = 0x1f00;
	uint16_t shift = 0x80;

	size_t ret = rtimd_eye_store(kobj, attr, buf, count,
		shift, reg1, reg2, slave_addr, data);
	return ret;
}

static DEVICE_ATTR_WO(rtimd_srd_param);
static DEVICE_ATTR_RW(rtimd_sreg);
static DEVICE_ATTR_WO(rtimd_brd_param);
static DEVICE_ATTR_RW(rtimd_breg);

static struct kobj_attribute left_eye_shift_left_attr =
	__ATTR(left_eye_shift_left, 0664,
			left_eye_shift_left_show, left_eye_shift_left_store);

static struct kobj_attribute left_eye_shift_right_attr =
	__ATTR(left_eye_shift_right, 0664,
			left_eye_shift_right_show, left_eye_shift_right_store);

static struct kobj_attribute left_eye_shift_top_attr =
	__ATTR(left_eye_shift_top, 0664,
			left_eye_shift_top_show, left_eye_shift_top_store);

static struct kobj_attribute left_eye_shift_bottom_attr =
	__ATTR(left_eye_shift_bottom, 0664,
			left_eye_shift_bottom_show, left_eye_shift_bottom_store);

static struct kobj_attribute right_eye_shift_left_attr =
	__ATTR(right_eye_shift_left, 0664,
			right_eye_shift_left_show, right_eye_shift_left_store);

static struct kobj_attribute right_eye_shift_right_attr =
	__ATTR(right_eye_shift_right, 0664,
			right_eye_shift_right_show, right_eye_shift_right_store);

static struct kobj_attribute right_eye_shift_top_attr =
	__ATTR(right_eye_shift_top, 0664,
			right_eye_shift_top_show, right_eye_shift_top_store);

static struct kobj_attribute right_eye_shift_bottom_attr =
	__ATTR(right_eye_shift_bottom, 0664,
			right_eye_shift_bottom_show, right_eye_shift_bottom_store);

static struct attribute *rtimd_attributes[] = {
	&left_eye_shift_left_attr.attr,
	&left_eye_shift_right_attr.attr,
	&left_eye_shift_top_attr.attr,
	&left_eye_shift_bottom_attr.attr,
	&right_eye_shift_left_attr.attr,
	&right_eye_shift_right_attr.attr,
	&right_eye_shift_top_attr.attr,
	&right_eye_shift_bottom_attr.attr,
	NULL,
};

static struct attribute_group rtimd_attribute_group = {
	.attrs = rtimd_attributes,
};

static int rtimd_sysfs_create(struct platform_device *pdev)
{
	int ret = 0;
	struct kset *rtimd_kset;
	struct kobject *rtimd_kobj;
	struct kobject *client_kobj;

	rtimd_kset = kset_create_and_add("rtimd", NULL, kernel_kobj);
	if (!rtimd_kset) {
		pr_err("rtimd_kset create failed\n");
		return -ENOMEM;
	}

	rtimd_kobj = &rtimd_kset->kobj;

	client_kobj = kobject_create_and_add("rtimd_eye", rtimd_kobj);
	if (!client_kobj) {
		pr_err("right_eye kobject_create_and_add failed\n");
		return -ENOMEM;
	}

	ret = sysfs_create_group(client_kobj, &rtimd_attribute_group);
	if (ret) {
		pr_err("[EX]: rtimd_eye sysfs_create_group() failed!!\n");
		sysfs_remove_group(client_kobj, &rtimd_attribute_group);
		return -ENOMEM;
	}

	return ret;
}

static int rtimd_probe(struct platform_device *pdev)
{
	int ret;

	pr_err("Enter\n");

	/* register the driver */
	if (register_chrdev(RTI_MD_MAJOR_NR, RTI_MD_DEV_NAME, &rtimd_fops)) {
		RMDERR("register_chrdev() failed (Major:%d).\n",
				RTI_MD_MAJOR_NR);
		pr_err("register_chrdev() %d\n", register_chrdev(RTI_MD_MAJOR_NR,
			RTI_MD_DEV_NAME, &rtimd_fops));
		return -EINVAL;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_rtimd_srd_param);
	ret |= device_create_file(&pdev->dev, &dev_attr_rtimd_sreg);
	ret |= device_create_file(&pdev->dev, &dev_attr_rtimd_brd_param);
	ret |= device_create_file(&pdev->dev, &dev_attr_rtimd_breg);
	if (ret) {
		RMDERR("Unable to create sysfs entries\n");

		/* un-register driver */
		unregister_chrdev(RTI_MD_MAJOR_NR, RTI_MD_DEV_NAME);
		return ret;
	}

	ret = rtimd_sysfs_create(pdev);

	pr_err("End\n");

	return 0;
}

static int rtimd_remove(struct platform_device *pdev)
{
	RMDDBG("\n");

	device_remove_file(&pdev->dev, &dev_attr_rtimd_srd_param);
	device_remove_file(&pdev->dev, &dev_attr_rtimd_sreg);
	device_remove_file(&pdev->dev, &dev_attr_rtimd_brd_param);
	device_remove_file(&pdev->dev, &dev_attr_rtimd_breg);

	/* un-register driver */
	unregister_chrdev(RTI_MD_MAJOR_NR, RTI_MD_DEV_NAME);

	return 0;
}

static int __init rtimd_dev_init(void)
{
	int retval = 0;
	int ret = 0;
	struct device *dev = NULL;

	pr_info("%s\n", __func__);

	rtimd_cb = kzalloc(sizeof(struct RTIMD_CB_T), GFP_KERNEL);
	if (!rtimd_cb)
		return -ENOMEM;

	ret = platform_driver_register(&rtimd_driver);
	if (ret != 0) {
		RMDERR("platform_driver_register failed.\n");
		kfree(rtimd_cb);
		return ret;
	}

	rtimd_device = platform_device_alloc(RTI_MD_DEV_NAME, -1);
	if (!rtimd_device) {
		RMDERR("platform_device_alloc() failed.\n");
		kfree(rtimd_cb);
		platform_driver_unregister(&rtimd_driver);
		return -ENOMEM;
	}

	/* add device */
	ret = platform_device_add(rtimd_device);
	if (ret) {
		RMDERR("platform_device_add() failed.\n");
		retval = ret;
		goto out;
	}

	/* create the node of device */
	rtimd_class = class_create(THIS_MODULE, RTI_MD_DEV_NAME);
	if (IS_ERR(rtimd_class)) {
		RMDERR("class_create() failed.\n");
		retval = PTR_ERR(rtimd_class);
		goto out;
	}

	/* create the logical device */
	dev = device_create(rtimd_class, NULL,
			MKDEV(RTI_MD_MAJOR_NR, RTI_MD_MINOR_NR), NULL,
			RTI_MD_DEV_NAME);
	if (IS_ERR(dev)) {
		RMDERR("device_create() failed.\n");
		retval = PTR_ERR(dev);
		goto out;
	}

	rtimd_cb->bus_num = -1; /* Set default bus number as invalid */
	return 0;

out:
	platform_device_put(rtimd_device);
	platform_driver_unregister(&rtimd_driver);

	kfree(rtimd_cb);
	rtimd_cb = NULL;

	return retval;
}

static void __exit rtimd_dev_exit(void)
{
	RMDDBG("\n");

	device_destroy(rtimd_class,
			MKDEV(RTI_MD_MAJOR_NR, RTI_MD_MINOR_NR));

	class_destroy(rtimd_class);

	platform_device_unregister(rtimd_device);

	platform_driver_unregister(&rtimd_driver);

	kfree(rtimd_cb->read_buf);

	kfree(rtimd_cb->write_buf);

	if (rtimd_cb->adap)
		i2c_put_adapter(rtimd_cb->adap);

	kfree(rtimd_cb);
	rtimd_cb = NULL;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("RAONTECH Inc.");
MODULE_DESCRIPTION("RAONTECH Micro Display I2C Driver");

module_init(rtimd_dev_init);
module_exit(rtimd_dev_exit);
